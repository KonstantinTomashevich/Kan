// It seems like we need this define for M_PI for MSVC.
#define _USE_MATH_DEFINES __CUSHION_PRESERVE__
#include <math.h>

#include <spirv/unified1/spirv.h>

#define KAN_RPL_COMPILER_IMPLEMENTATION
#include <kan/render_pipeline_language/compiler_internal.h>

struct spirv_arbitrary_instruction_item_t
{
    struct spirv_arbitrary_instruction_item_t *next;
    spirv_size_t code[];
};

struct spirv_arbitrary_instruction_section_t
{
    struct spirv_arbitrary_instruction_item_t *first;
    struct spirv_arbitrary_instruction_item_t *last;
};

struct spirv_generation_array_type_t
{
    struct spirv_generation_array_type_t *next;
    spirv_size_t spirv_id;
    struct compiler_instance_type_definition_t type;
};

struct spirv_generation_function_type_t
{
    struct spirv_generation_function_type_t *next;
    kan_instance_size_t argument_count;
    spirv_size_t generated_id;
    spirv_size_t return_type_id;
    spirv_size_t *argument_types;
};

struct spirv_block_persistent_load_t
{
    struct spirv_block_persistent_load_t *next;
    spirv_size_t variable_id;
    spirv_size_t token_id;
};

struct spirv_generation_block_t
{
    struct spirv_generation_block_t *next;
    spirv_size_t spirv_id;

    /// \details We need to store variables in the first function block right after the label.
    ///          Therefore label and variables have separate section.
    struct spirv_arbitrary_instruction_section_t header_section;

    struct spirv_arbitrary_instruction_section_t code_section;

    struct spirv_block_persistent_load_t *first_persistent_load;
};

struct spirv_generation_temporary_variable_t
{
    struct spirv_generation_temporary_variable_t *next;
    spirv_size_t spirv_id;
    spirv_size_t spirv_type_id;
};

struct spirv_generation_builtin_used_by_stage_t
{
    struct spirv_generation_builtin_used_by_stage_t *next;
    enum kan_rpl_pipeline_stage_t stage;
};

struct spirv_generation_builtin_t
{
    struct spirv_generation_builtin_t *next;
    spirv_size_t spirv_id;
    SpvBuiltIn builtin_type;
    SpvStorageClass builtin_storage;
    struct spirv_generation_builtin_used_by_stage_t *first_stage;
};

struct spirv_generation_function_node_t
{
    struct spirv_generation_function_node_t *next;
    struct compiler_instance_function_node_t *source;
    struct spirv_arbitrary_instruction_section_t header_section;

    struct spirv_generation_block_t *first_block;
    struct spirv_generation_block_t *last_block;

    /// \details End section is an utility that only contains function end.
    struct spirv_arbitrary_instruction_section_t end_section;

    struct spirv_generation_temporary_variable_t *first_free_temporary_variable;
    struct spirv_generation_temporary_variable_t *first_used_temporary_variable;
};

struct spirv_generation_floating_constant_t
{
    struct spirv_generation_floating_constant_t *next;
    spirv_size_t spirv_id;
    float value;
};

struct spirv_generation_unsigned_constant_t
{
    struct spirv_generation_unsigned_constant_t *next;
    spirv_size_t spirv_id;
    spirv_unsigned_literal_t value;
};

struct spirv_generation_signed_constant_t
{
    struct spirv_generation_signed_constant_t *next;
    spirv_size_t spirv_id;
    spirv_signed_literal_t value;
};

struct spirv_known_pointer_type_t
{
    struct spirv_known_pointer_type_t *next;
    spirv_size_t source_type_id;
    spirv_size_t pointer_type_id;
};

struct spirv_image_type_identifiers_t
{
    spirv_size_t image;
    spirv_size_t sampled_image;
};

struct spirv_generation_context_t
{
    struct rpl_compiler_instance_t *instance;
    spirv_size_t current_bound;
    kan_instance_size_t code_word_count;
    kan_bool_t emit_result;

    struct kan_stack_group_allocator_t temporary_allocator;

    struct spirv_arbitrary_instruction_section_t debug_section;
    struct spirv_arbitrary_instruction_section_t decoration_section;
    struct spirv_arbitrary_instruction_section_t base_type_section;
    struct spirv_arbitrary_instruction_section_t higher_type_section;
    struct spirv_arbitrary_instruction_section_t global_variable_section;

    struct spirv_generation_function_node_t *first_function_node;
    struct spirv_generation_function_node_t *last_function_node;

    struct spirv_generation_array_type_t *first_generated_array_type;
    struct spirv_generation_function_type_t *first_generated_function_type;
    struct spirv_generation_builtin_t *first_builtin;

    struct spirv_generation_floating_constant_t *first_floating_constant;
    struct spirv_generation_unsigned_constant_t *first_unsigned_constant;
    struct spirv_generation_signed_constant_t *first_signed_constant;

    struct spirv_known_pointer_type_t *first_known_input_pointer;
    struct spirv_known_pointer_type_t *first_known_output_pointer;
    struct spirv_known_pointer_type_t *first_known_uniform_pointer;
    struct spirv_known_pointer_type_t *first_known_uniform_constant_pointer;
    struct spirv_known_pointer_type_t *first_known_storage_buffer_pointer;
    struct spirv_known_pointer_type_t *first_known_push_constant_pointer;
    struct spirv_known_pointer_type_t *first_known_function_pointer;

    kan_bool_t extension_requirement_non_uniform;

    spirv_size_t vector_ids[INBUILT_VECTOR_TYPE_COUNT];
    spirv_size_t matrix_ids[INBUILT_MATRIX_TYPE_COUNT];
    spirv_size_t sampler_id;
    struct spirv_image_type_identifiers_t image_ids[KAN_RPL_IMAGE_TYPE_COUNT];
};

static inline spirv_size_t *spirv_new_instruction (struct spirv_generation_context_t *context,
                                                   struct spirv_arbitrary_instruction_section_t *section,
                                                   kan_instance_size_t word_count)
{
    struct spirv_arbitrary_instruction_item_t *item = kan_stack_group_allocator_allocate (
        &context->temporary_allocator,
        sizeof (struct spirv_arbitrary_instruction_item_t) + sizeof (spirv_size_t) * word_count,
        _Alignof (struct spirv_arbitrary_instruction_item_t));

    item->next = NULL;
    item->code[0u] = ((spirv_size_t) word_count) << SpvWordCountShift;

    if (section->last)
    {
        section->last->next = item;
        section->last = item;
    }
    else
    {
        section->first = item;
        section->last = item;
    }

    context->code_word_count += (spirv_size_t) word_count;
    return item->code;
}

static inline spirv_size_t spirv_to_word_length (spirv_size_t length)
{
    return (length + 1u) % sizeof (spirv_size_t) == 0u ? (length + 1u) / sizeof (spirv_size_t) :
                                                         1u + (length + 1u) / sizeof (spirv_size_t);
}

static inline void spirv_generate_op_name (struct spirv_generation_context_t *context,
                                           spirv_size_t for_id,
                                           const char *name)
{
    const spirv_size_t length = (spirv_size_t) strlen (name);
    const spirv_size_t word_length = spirv_to_word_length (length);
    spirv_size_t *code = spirv_new_instruction (context, &context->debug_section, 2u + word_length);
    code[0u] |= SpvOpCodeMask & SpvOpName;
    code[1u] = for_id;
    code[1u + word_length] = 0u;
    memcpy ((uint8_t *) (code + 2u), name, length);
}

static inline void spirv_generate_op_member_name (struct spirv_generation_context_t *context,
                                                  spirv_size_t struct_id,
                                                  spirv_size_t member_index,
                                                  const char *name)
{
    const spirv_size_t length = (spirv_size_t) strlen (name);
    const spirv_size_t word_length = spirv_to_word_length (length);
    spirv_size_t *code = spirv_new_instruction (context, &context->debug_section, 3u + word_length);
    code[0u] |= SpvOpCodeMask & SpvOpMemberName;
    code[1u] = struct_id;
    code[2u] = member_index;
    code[2u + word_length] = 0u;
    memcpy ((uint8_t *) (code + 3u), name, length);
}

static inline void spirv_register_and_generate_known_pointer_type (
    struct spirv_generation_context_t *context,
    struct spirv_arbitrary_instruction_section_t *section,
    spirv_size_t expected_pointer_id,
    spirv_size_t base_type_id,
    SpvStorageClass storage_class)

{
    spirv_size_t *code = spirv_new_instruction (context, section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = expected_pointer_id;
    code[2u] = storage_class;
    code[3u] = base_type_id;

    struct spirv_known_pointer_type_t *new_type =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&context->temporary_allocator, struct spirv_known_pointer_type_t);

    new_type->pointer_type_id = expected_pointer_id;
    new_type->source_type_id = base_type_id;

    switch (storage_class)
    {
    case SpvStorageClassInput:
        new_type->next = context->first_known_input_pointer;
        context->first_known_input_pointer = new_type;
        break;

    case SpvStorageClassOutput:
        new_type->next = context->first_known_output_pointer;
        context->first_known_output_pointer = new_type;
        break;

    case SpvStorageClassUniform:
        new_type->next = context->first_known_uniform_pointer;
        context->first_known_uniform_pointer = new_type;
        break;

    case SpvStorageClassUniformConstant:
        new_type->next = context->first_known_uniform_constant_pointer;
        context->first_known_uniform_constant_pointer = new_type;
        break;

    case SpvStorageClassStorageBuffer:
        new_type->next = context->first_known_storage_buffer_pointer;
        context->first_known_storage_buffer_pointer = new_type;
        break;

    case SpvStorageClassPushConstant:
        new_type->next = context->first_known_push_constant_pointer;
        context->first_known_push_constant_pointer = new_type;
        break;

    case SpvStorageClassFunction:
        new_type->next = context->first_known_function_pointer;
        context->first_known_function_pointer = new_type;
        break;

    default:
        // Define unsupported.
        KAN_ASSERT (KAN_FALSE)
        break;
    }
}

static spirv_size_t spirv_get_or_create_pointer_type (struct spirv_generation_context_t *context,
                                                      spirv_size_t base_type_id,
                                                      SpvStorageClass storage_class)
{
    struct spirv_known_pointer_type_t *known;
    switch (storage_class)
    {
    case SpvStorageClassInput:
        known = context->first_known_input_pointer;
        break;

    case SpvStorageClassOutput:
        known = context->first_known_output_pointer;
        break;

    case SpvStorageClassUniform:
        known = context->first_known_uniform_pointer;
        break;

    case SpvStorageClassUniformConstant:
        known = context->first_known_uniform_constant_pointer;
        break;

    case SpvStorageClassStorageBuffer:
        known = context->first_known_storage_buffer_pointer;
        break;

    case SpvStorageClassPushConstant:
        known = context->first_known_push_constant_pointer;
        break;

    case SpvStorageClassFunction:
        known = context->first_known_function_pointer;
        break;

    default:
        // Define unsupported.
        KAN_ASSERT (KAN_FALSE)
        return (spirv_size_t) SPIRV_FIXED_ID_INVALID;
    }

    while (known)
    {
        if (known->source_type_id == base_type_id)
        {
            return known->pointer_type_id;
        }

        known = known->next;
    }

    spirv_size_t new_id = context->current_bound;
    ++context->current_bound;
    spirv_register_and_generate_known_pointer_type (context, &context->higher_type_section, new_id, base_type_id,
                                                    storage_class);
    return new_id;
}

static void spirv_generate_standard_types (struct spirv_generation_context_t *context)
{
    // We intentionally do not generate special names for pointers.
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_VOID, "void");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_BOOLEAN, "bool");

    spirv_size_t *code = spirv_new_instruction (context, &context->base_type_section, 2u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVoid;
    code[1u] = SPIRV_FIXED_ID_TYPE_VOID;

    code = spirv_new_instruction (context, &context->base_type_section, 2u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeBool;
    code[1u] = SPIRV_FIXED_ID_TYPE_BOOLEAN;
}

static void spirv_init_generation_context (struct spirv_generation_context_t *context,
                                           struct rpl_compiler_instance_t *instance)
{
    context->instance = instance;
    context->current_bound = (spirv_size_t) SPIRV_FIXED_ID_END;
    context->code_word_count = 0u;
    context->emit_result = KAN_TRUE;

    kan_stack_group_allocator_init (&context->temporary_allocator, STATICS.rpl_compiler_instance_allocation_group,
                                    KAN_RPL_PARSER_SPIRV_GENERATION_TEMPORARY_SIZE);

    context->debug_section.first = NULL;
    context->debug_section.last = NULL;
    context->decoration_section.first = NULL;
    context->decoration_section.last = NULL;
    context->base_type_section.first = NULL;
    context->base_type_section.last = NULL;
    context->higher_type_section.first = NULL;
    context->higher_type_section.last = NULL;
    context->global_variable_section.first = NULL;
    context->global_variable_section.last = NULL;
    context->first_function_node = NULL;
    context->last_function_node = NULL;

    context->first_generated_array_type = NULL;
    context->first_generated_function_type = NULL;

    context->first_floating_constant = NULL;
    context->first_unsigned_constant = NULL;
    context->first_signed_constant = NULL;
    context->first_builtin = NULL;

    context->first_known_input_pointer = NULL;
    context->first_known_output_pointer = NULL;
    context->first_known_uniform_pointer = NULL;
    context->first_known_uniform_constant_pointer = NULL;
    context->first_known_storage_buffer_pointer = NULL;
    context->first_known_push_constant_pointer = NULL;
    context->first_known_function_pointer = NULL;

    context->extension_requirement_non_uniform = KAN_FALSE;
    spirv_generate_standard_types (context);

    for (kan_loop_size_t index = 0u; index < INBUILT_VECTOR_TYPE_COUNT; ++index)
    {
        context->vector_ids[index] = SPIRV_FIXED_ID_INVALID;
    }

    for (kan_loop_size_t index = 0u; index < INBUILT_MATRIX_TYPE_COUNT; ++index)
    {
        context->matrix_ids[index] = SPIRV_FIXED_ID_INVALID;
    }

    context->sampler_id = SPIRV_FIXED_ID_INVALID;
    for (kan_loop_size_t index = 0u; index < KAN_RPL_IMAGE_TYPE_COUNT; ++index)
    {
        context->image_ids[index].image = SPIRV_FIXED_ID_INVALID;
        context->image_ids[index].sampled_image = SPIRV_FIXED_ID_INVALID;
    }
}

static inline void spirv_copy_instructions (spirv_size_t **output,
                                            struct spirv_arbitrary_instruction_item_t *instruction_item)
{
    while (instruction_item)
    {
        const spirv_size_t word_count = (*instruction_item->code & ~SpvOpCodeMask) >> SpvWordCountShift;
        memcpy (*output, instruction_item->code, word_count * sizeof (spirv_size_t));
        *output += word_count;
        instruction_item = instruction_item->next;
    }
}

static kan_bool_t spirv_finalize_generation_context (struct spirv_generation_context_t *context,
                                                     struct kan_dynamic_array_t *code_output)
{
    struct spirv_arbitrary_instruction_section_t base_section;
    base_section.first = NULL;
    base_section.last = NULL;

    switch (context->instance->pipeline_type)
    {
    case KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC:
    {
        spirv_size_t *code = spirv_new_instruction (context, &base_section, 2u);
        *code |= SpvOpCodeMask & SpvOpCapability;
        *(code + 1u) = SpvCapabilityShader;

        if (context->extension_requirement_non_uniform)
        {
            code = spirv_new_instruction (context, &base_section, 2u);
            *code |= SpvOpCodeMask & SpvOpCapability;
            *(code + 1u) = SpvCapabilityShaderNonUniformEXT;

            static const char shader_non_uniform_extension_padded[] = "SPV_EXT_descriptor_indexing";
            _Static_assert (sizeof (shader_non_uniform_extension_padded) % sizeof (spirv_size_t) == 0u,
                            "GLSL library name is really padded.");

            code = spirv_new_instruction (context, &base_section,
                                          1u + sizeof (shader_non_uniform_extension_padded) / sizeof (spirv_size_t));
            code[0u] |= SpvOpCodeMask & SpvOpExtension;
            memcpy (&code[1u], shader_non_uniform_extension_padded, sizeof (shader_non_uniform_extension_padded));
        }

        break;
    }
    }

    static const char glsl_library_padded[] = "GLSL.std.450\0\0\0";
    _Static_assert (sizeof (glsl_library_padded) % sizeof (spirv_size_t) == 0u, "GLSL library name is really padded.");
    spirv_size_t *op_glsl_import =
        spirv_new_instruction (context, &base_section, 2u + sizeof (glsl_library_padded) / sizeof (spirv_size_t));
    op_glsl_import[0u] |= SpvOpCodeMask & SpvOpExtInstImport;
    op_glsl_import[1u] = (spirv_size_t) SPIRV_FIXED_ID_GLSL_LIBRARY;
    memcpy (&op_glsl_import[2u], glsl_library_padded, sizeof (glsl_library_padded));

    switch (context->instance->pipeline_type)
    {
    case KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC:
    {
        spirv_size_t *op_memory_model = spirv_new_instruction (context, &base_section, 3u);
        op_memory_model[0u] |= SpvOpCodeMask & SpvOpMemoryModel;
        op_memory_model[1u] = SpvAddressingModelLogical;
        op_memory_model[2u] = SpvMemoryModelGLSL450;
        break;
    }
    }

    for (kan_loop_size_t entry_point_index = 0u; entry_point_index < context->instance->entry_point_count;
         ++entry_point_index)
    {
        struct kan_rpl_entry_point_t *entry_point = &context->instance->entry_points[entry_point_index];
        struct compiler_instance_function_node_t *function = context->instance->first_function;

        while (function)
        {
            if (function->name == entry_point->function_name)
            {
                break;
            }

            function = function->next;
        }

        if (!function)
        {
            KAN_LOG (rpl_compiler_instance, KAN_LOG_ERROR,
                     "[%s] Failed to generate entry point: unable to find function \"%s\"",
                     context->instance->context_log_name, entry_point->function_name)
            context->emit_result = KAN_FALSE;
            continue;
        }

        kan_loop_size_t accesses_count = 0u;
        struct compiler_instance_container_access_node_t *container_access = function->first_container_access;

        while (container_access)
        {
            struct compiler_instance_container_field_node_t *field = container_access->container->first_field;
            while (field)
            {
                struct compiler_instance_container_field_stage_node_t *stage = field->first_usage_stage;
                while (stage)
                {
                    if (stage->user_stage == entry_point->stage)
                    {
                        if (stage->spirv_id_input != SPIRV_FIXED_ID_INVALID)
                        {
                            ++accesses_count;
                        }

                        if (stage->spirv_id_output != SPIRV_FIXED_ID_INVALID)
                        {
                            ++accesses_count;
                        }
                    }

                    stage = stage->next;
                }

                field = field->next;
            }

            container_access = container_access->next;
        }

        struct spirv_generation_builtin_t *builtin = context->first_builtin;
        while (builtin)
        {
            struct spirv_generation_builtin_used_by_stage_t *usage = builtin->first_stage;
            while (usage)
            {
                if (usage->stage == entry_point->stage)
                {
                    ++accesses_count;
                    break;
                }

                usage = usage->next;
            }

            builtin = builtin->next;
        }

        const spirv_size_t name_length = (spirv_size_t) strlen (function->name);
        const spirv_size_t name_word_length = spirv_to_word_length (name_length);

        spirv_size_t *entry_point_code =
            spirv_new_instruction (context, &base_section, 3u + name_word_length + accesses_count);
        entry_point_code[0u] |= SpvOpCodeMask & SpvOpEntryPoint;

        switch (entry_point->stage)
        {
        case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX:
            entry_point_code[1u] = SpvExecutionModelVertex;
            break;

        case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT:
            entry_point_code[1u] = SpvExecutionModelFragment;
            break;
        }

        entry_point_code[2u] = function->spirv_id;
        entry_point_code[2u + name_word_length] = 0u;
        memcpy ((uint8_t *) (entry_point_code + 3u), function->name, name_length);
        spirv_size_t *access_output = entry_point_code + 3u + name_word_length;
        container_access = function->first_container_access;

        while (container_access)
        {
            struct compiler_instance_container_field_node_t *field = container_access->container->first_field;
            while (field)
            {
                struct compiler_instance_container_field_stage_node_t *stage = field->first_usage_stage;
                while (stage)
                {
                    if (stage->user_stage == entry_point->stage)
                    {
                        if (stage->spirv_id_input != SPIRV_FIXED_ID_INVALID)
                        {
                            *access_output = stage->spirv_id_input;
                            ++access_output;
                        }

                        if (stage->spirv_id_output != SPIRV_FIXED_ID_INVALID)
                        {
                            *access_output = stage->spirv_id_output;
                            ++access_output;
                        }
                    }

                    stage = stage->next;
                }

                field = field->next;
            }

            container_access = container_access->next;
        }

        builtin = context->first_builtin;
        while (builtin)
        {
            struct spirv_generation_builtin_used_by_stage_t *usage = builtin->first_stage;
            while (usage)
            {
                if (usage->stage == entry_point->stage)
                {
                    *access_output = builtin->spirv_id;
                    ++access_output;
                    break;
                }

                usage = usage->next;
            }

            builtin = builtin->next;
        }

        switch (entry_point->stage)
        {
        case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX:
            break;

        case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT:
        {
            spirv_size_t *execution_mode_code = spirv_new_instruction (context, &base_section, 3u);
            execution_mode_code[0u] |= SpvOpCodeMask & SpvOpExecutionMode;
            execution_mode_code[1u] = function->spirv_id;
            execution_mode_code[2u] = SpvExecutionModeOriginUpperLeft;
            break;
        }
        }
    }

    kan_dynamic_array_set_capacity (code_output, 5u + context->code_word_count);
    code_output->size = code_output->capacity;

    spirv_size_t *output = (spirv_size_t *) code_output->data;
    output[0u] = SpvMagicNumber;
    output[1u] = 0x00010300;
    output[2u] = 0u;
    output[3u] = context->current_bound;
    output[4u] = 0u;
    output += 5u;

    spirv_copy_instructions (&output, base_section.first);
    spirv_copy_instructions (&output, context->debug_section.first);
    spirv_copy_instructions (&output, context->decoration_section.first);
    spirv_copy_instructions (&output, context->base_type_section.first);
    spirv_copy_instructions (&output, context->higher_type_section.first);
    spirv_copy_instructions (&output, context->global_variable_section.first);

    struct spirv_generation_function_node_t *function_node = context->first_function_node;
    while (function_node)
    {
        spirv_copy_instructions (&output, function_node->header_section.first);
        struct spirv_generation_block_t *block = function_node->first_block;

        while (block)
        {
            spirv_copy_instructions (&output, block->header_section.first);
            spirv_copy_instructions (&output, block->code_section.first);
            block = block->next;
        }

        spirv_copy_instructions (&output, function_node->end_section.first);
        function_node = function_node->next;
    }

    kan_stack_group_allocator_shutdown (&context->temporary_allocator);
    return context->emit_result;
}

static spirv_size_t spirv_find_or_generate_vector_type (struct spirv_generation_context_t *context,
                                                        kan_rpl_size_t type_index);

static spirv_size_t spirv_request_f1_constant (struct spirv_generation_context_t *context, float value)
{
    struct spirv_generation_floating_constant_t *existent_constant = context->first_floating_constant;
    while (existent_constant)
    {
        if (existent_constant->value == value)
        {
            return existent_constant->spirv_id;
        }

        existent_constant = existent_constant->next;
    }

    spirv_size_t constant_id = context->current_bound;
    ++context->current_bound;
    spirv_size_t constant_type =
        spirv_find_or_generate_vector_type (context, INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_FLOAT, 1u));

    spirv_size_t *constant_code = spirv_new_instruction (context, &context->base_type_section, 4u);
    constant_code[0u] |= SpvOpCodeMask & SpvOpConstant;
    constant_code[1u] = constant_type;
    constant_code[2u] = constant_id;
    *(float *) &constant_code[3u] = value;

    struct spirv_generation_floating_constant_t *new_constant = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &context->temporary_allocator, struct spirv_generation_floating_constant_t);

    new_constant->next = context->first_floating_constant;
    context->first_floating_constant = new_constant;
    new_constant->spirv_id = constant_id;
    new_constant->value = value;
    return constant_id;
}

static spirv_size_t spirv_request_u1_constant (struct spirv_generation_context_t *context,
                                               spirv_unsigned_literal_t value)
{
    struct spirv_generation_unsigned_constant_t *existent_constant = context->first_unsigned_constant;
    while (existent_constant)
    {
        if (existent_constant->value == value)
        {
            return existent_constant->spirv_id;
        }

        existent_constant = existent_constant->next;
    }

    spirv_size_t constant_id = context->current_bound;
    ++context->current_bound;
    spirv_size_t constant_type =
        spirv_find_or_generate_vector_type (context, INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_UNSIGNED, 1u));

    spirv_size_t *constant_code = spirv_new_instruction (context, &context->base_type_section, 4u);
    constant_code[0u] |= SpvOpCodeMask & SpvOpConstant;
    constant_code[1u] = constant_type;
    constant_code[2u] = constant_id;
    *(spirv_unsigned_literal_t *) &constant_code[3u] = (spirv_unsigned_literal_t) value;

    struct spirv_generation_unsigned_constant_t *new_constant = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &context->temporary_allocator, struct spirv_generation_unsigned_constant_t);

    new_constant->next = context->first_unsigned_constant;
    context->first_unsigned_constant = new_constant;
    new_constant->spirv_id = constant_id;
    new_constant->value = value;
    return constant_id;
}

static spirv_size_t spirv_request_s1_constant (struct spirv_generation_context_t *context, spirv_signed_literal_t value)
{
    struct spirv_generation_signed_constant_t *existent_constant = context->first_signed_constant;
    while (existent_constant)
    {
        if (existent_constant->value == value)
        {
            return existent_constant->spirv_id;
        }

        existent_constant = existent_constant->next;
    }

    spirv_size_t constant_id = context->current_bound;
    ++context->current_bound;
    spirv_size_t constant_type =
        spirv_find_or_generate_vector_type (context, INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_SIGNED, 1u));

    spirv_size_t *constant_code = spirv_new_instruction (context, &context->base_type_section, 4u);
    constant_code[0u] |= SpvOpCodeMask & SpvOpConstant;
    constant_code[1u] = constant_type;
    constant_code[2u] = constant_id;
    *(spirv_signed_literal_t *) &constant_code[3u] = (spirv_signed_literal_t) value;

    struct spirv_generation_signed_constant_t *new_constant = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &context->temporary_allocator, struct spirv_generation_signed_constant_t);

    new_constant->next = context->first_signed_constant;
    context->first_signed_constant = new_constant;
    new_constant->spirv_id = constant_id;
    new_constant->value = value;
    return constant_id;
}

static spirv_size_t spirv_find_or_generate_vector_type (struct spirv_generation_context_t *context,
                                                        kan_rpl_size_t type_index)
{
    if (context->vector_ids[type_index] == SPIRV_FIXED_ID_INVALID)
    {
        struct inbuilt_vector_type_t *type = &STATICS.vector_types[type_index];
        if (type->items_count == 1u)
        {
            context->vector_ids[type_index] = context->current_bound;
            ++context->current_bound;

            switch (type->item)
            {
            case INBUILT_TYPE_ITEM_FLOAT:
            {
                spirv_size_t *code = spirv_new_instruction (context, &context->base_type_section, 3u);
                code[0u] |= SpvOpCodeMask & SpvOpTypeFloat;
                code[1u] = context->vector_ids[type_index];
                code[2u] = 32u;
                break;
            }

            case INBUILT_TYPE_ITEM_UNSIGNED:
            {
                spirv_size_t *code = spirv_new_instruction (context, &context->base_type_section, 4u);
                code[0u] |= SpvOpCodeMask & SpvOpTypeInt;
                code[1u] = context->vector_ids[type_index];
                code[2u] = 32u;
                code[3u] = 0u; // Unsigned.
                break;
            }

            case INBUILT_TYPE_ITEM_SIGNED:
            {
                spirv_size_t *code = spirv_new_instruction (context, &context->base_type_section, 4u);
                code[0u] |= SpvOpCodeMask & SpvOpTypeInt;
                code[1u] = context->vector_ids[type_index];
                code[2u] = 32u;
                code[3u] = 1u; // Signed.
                break;
            }
            }
        }
        else
        {
            spirv_size_t item_type_id =
                spirv_find_or_generate_vector_type (context, INBUILT_VECTOR_TYPE_INDEX (type->item, 1u));

            context->vector_ids[type_index] = context->current_bound;
            ++context->current_bound;

            spirv_size_t *code = spirv_new_instruction (context, &context->base_type_section, 4u);
            code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
            code[1u] = context->vector_ids[type_index];
            code[2u] = item_type_id;
            code[3u] = type->items_count;
        }

        spirv_generate_op_name (context, context->vector_ids[type_index], type->name);
    }

    return context->vector_ids[type_index];
}

static spirv_size_t spirv_find_or_generate_matrix_type (struct spirv_generation_context_t *context,
                                                        kan_rpl_size_t type_index)
{
    if (context->matrix_ids[type_index] == SPIRV_FIXED_ID_INVALID)
    {
        struct inbuilt_matrix_type_t *type = &STATICS.matrix_types[type_index];
        spirv_size_t column_type_id =
            spirv_find_or_generate_vector_type (context, INBUILT_VECTOR_TYPE_INDEX (type->item, type->rows));

        context->matrix_ids[type_index] = context->current_bound;
        ++context->current_bound;
        spirv_generate_op_name (context, context->matrix_ids[type_index], type->name);

        spirv_size_t *code = spirv_new_instruction (context, &context->base_type_section, 4u);
        code[0u] |= SpvOpCodeMask & SpvOpTypeMatrix;
        code[1u] = context->matrix_ids[type_index];
        code[2u] = column_type_id;
        code[3u] = type->columns;
    }

    return context->matrix_ids[type_index];
}

static spirv_size_t spirv_find_or_generate_sampler_type (struct spirv_generation_context_t *context)
{
    if (context->sampler_id == SPIRV_FIXED_ID_INVALID)
    {
        context->sampler_id = context->current_bound;
        ++context->current_bound;
        spirv_generate_op_name (context, context->sampler_id, "sampler");

        spirv_size_t *code = spirv_new_instruction (context, &context->base_type_section, 2u);
        code[0u] |= SpvOpCodeMask & SpvOpTypeSampler;
        code[1u] = context->sampler_id;
    }

    return context->sampler_id;
}

static struct spirv_image_type_identifiers_t spirv_find_or_generate_image_type (
    struct spirv_generation_context_t *context, enum kan_rpl_image_type_t image_type)
{
    if (context->image_ids[image_type].image == SPIRV_FIXED_ID_INVALID)
    {
        context->image_ids[image_type].image = context->current_bound;
        ++context->current_bound;
        const char *image_type_name = NULL;
        const char *sampled_image_type_name = NULL;

        spirv_size_t *code = spirv_new_instruction (context, &context->base_type_section, 9u);
        code[0u] |= SpvOpCodeMask & SpvOpTypeImage;
        code[1u] = context->image_ids[image_type].image;
        code[2u] =
            spirv_find_or_generate_vector_type (context, INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_FLOAT, 1u));
        code[6u] = 0u; // 0 if single sampled, 1 if multi sampled.
        code[7u] = 1u; // 1 if sampled, 2 if used for read-write, 0 if unknown.
        code[8u] = SpvImageFormatUnknown;

        switch (image_type)
        {
        case KAN_RPL_IMAGE_TYPE_COLOR_2D:
            code[3u] = SpvDim2D; // Dimensions.
            code[4u] = 0u;       // 0 if color image, 1 if depth image, 2 if unknown.
            code[5u] = 0u;       // 0 if not arrayed, 1 if arrayed.
            image_type_name = "image_color_2d";
            sampled_image_type_name = "sampled_image_color_2d";
            break;

        case KAN_RPL_IMAGE_TYPE_COLOR_3D:
            code[3u] = SpvDim3D; // Dimensions.
            code[4u] = 0u;       // 0 if color image, 1 if depth image, 2 if unknown.
            code[5u] = 0u;       // 0 if not arrayed, 1 if arrayed.
            image_type_name = "image_color_3d";
            sampled_image_type_name = "sampled_image_color_3d";
            break;

        case KAN_RPL_IMAGE_TYPE_COLOR_CUBE:
            code[3u] = SpvDimCube; // Dimensions.
            code[4u] = 0u;         // 0 if color image, 1 if depth image, 2 if unknown.
            code[5u] = 0u;         // 0 if not arrayed, 1 if arrayed.
            image_type_name = "image_color_cube";
            sampled_image_type_name = "sampled_image_color_cube";
            break;

        case KAN_RPL_IMAGE_TYPE_COLOR_2D_ARRAY:
            code[3u] = SpvDim2D; // Dimensions.
            code[4u] = 0u;       // 0 if color image, 1 if depth image, 2 if unknown.
            code[5u] = 1u;       // 0 if not arrayed, 1 if arrayed.
            image_type_name = "image_color_2d_array";
            sampled_image_type_name = "sampled_image_color_2d_array";
            break;

        case KAN_RPL_IMAGE_TYPE_DEPTH_2D:
            code[3u] = SpvDim2D; // Dimensions.
            code[4u] = 1u;       // 0 if color image, 1 if depth image, 2 if unknown.
            code[5u] = 0u;       // 0 if not arrayed, 1 if arrayed.
            image_type_name = "image_depth_2d";
            sampled_image_type_name = "sampled_image_depth_2d";
            break;

        case KAN_RPL_IMAGE_TYPE_DEPTH_3D:
            code[3u] = SpvDim3D; // Dimensions.
            code[4u] = 1u;       // 0 if color image, 1 if depth image, 2 if unknown.
            code[5u] = 0u;       // 0 if not arrayed, 1 if arrayed.
            image_type_name = "image_depth_3d";
            sampled_image_type_name = "sampled_image_depth_3d";
            break;

        case KAN_RPL_IMAGE_TYPE_DEPTH_CUBE:
            code[3u] = SpvDimCube; // Dimensions.
            code[4u] = 1u;         // 0 if color image, 1 if depth image, 2 if unknown.
            code[5u] = 0u;         // 0 if not arrayed, 1 if arrayed.
            image_type_name = "image_depth_cube";
            sampled_image_type_name = "sampled_image_depth_cube";
            break;

        case KAN_RPL_IMAGE_TYPE_DEPTH_2D_ARRAY:
            code[3u] = SpvDim2D; // Dimensions.
            code[4u] = 1u;       // 0 if color image, 1 if depth image, 2 if unknown.
            code[5u] = 1u;       // 0 if not arrayed, 1 if arrayed.
            image_type_name = "image_depth_2d_array";
            sampled_image_type_name = "sampled_image_depth_2d_array";
            break;

        case KAN_RPL_IMAGE_TYPE_COUNT:
            KAN_ASSERT (KAN_FALSE)
            break;
        }

        context->image_ids[image_type].sampled_image = context->current_bound;
        ++context->current_bound;

        code = spirv_new_instruction (context, &context->base_type_section, 3u);
        code[0u] |= SpvOpCodeMask & SpvOpTypeSampledImage;
        code[1u] = context->image_ids[image_type].sampled_image;
        code[2u] = context->image_ids[image_type].image;

        spirv_generate_op_name (context, context->image_ids[image_type].image, image_type_name);
        spirv_generate_op_name (context, context->image_ids[image_type].sampled_image, sampled_image_type_name);
    }

    return context->image_ids[image_type];
}

static spirv_size_t spirv_find_or_generate_object_type (struct spirv_generation_context_t *context,
                                                        struct compiler_instance_type_definition_t *type,
                                                        kan_loop_size_t start_dimension_index)
{
    if ((!type->array_size_runtime && start_dimension_index == type->array_dimensions_count) ||
        (type->array_size_runtime && start_dimension_index == 1u))
    {
        switch (type->class)
        {
        case COMPILER_INSTANCE_TYPE_CLASS_VOID:
            return SPIRV_FIXED_ID_TYPE_VOID;

        case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
            return spirv_find_or_generate_vector_type (context,
                                                       (kan_rpl_size_t) (type->vector_data - STATICS.vector_types));

        case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
            return spirv_find_or_generate_matrix_type (context,
                                                       (kan_rpl_size_t) (type->matrix_data - STATICS.matrix_types));

        case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
            // Should be already registered at the start when all used structures are being registered.
            KAN_ASSERT (type->struct_data->spirv_id_value != SPIRV_FIXED_ID_INVALID)
            return type->struct_data->spirv_id_value;

        case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
            return SPIRV_FIXED_ID_TYPE_BOOLEAN;

        case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
            // Cannot be used here.
            KAN_ASSERT (KAN_FALSE)
            break;

        case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
            return spirv_find_or_generate_sampler_type (context);

        case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
            return spirv_find_or_generate_image_type (context, type->image_type).image;
        }

        KAN_ASSERT (KAN_FALSE)
    }

    struct spirv_generation_array_type_t *array_type = context->first_generated_array_type;
    while (array_type)
    {
        if (is_type_definition_base_equal (&array_type->type, type) &&
            array_type->type.array_size_runtime == type->array_size_runtime &&
            array_type->type.array_dimensions_count == type->array_dimensions_count - start_dimension_index &&
            memcmp (array_type->type.array_dimensions, &type->array_dimensions[start_dimension_index],
                    array_type->type.array_dimensions_count * sizeof (kan_rpl_size_t)) == 0)
        {
            return array_type->spirv_id;
        }

        array_type = array_type->next;
    }

    const spirv_size_t base_type_id = spirv_find_or_generate_object_type (context, type, start_dimension_index + 1u);
    spirv_size_t array_type_id = context->current_bound;
    ++context->current_bound;

    if (type->array_size_runtime)
    {
        spirv_size_t *runtime_array_code = spirv_new_instruction (context, &context->higher_type_section, 3u);
        runtime_array_code[0u] |= SpvOpCodeMask & SpvOpTypeRuntimeArray;
        runtime_array_code[1u] = array_type_id;
        runtime_array_code[2u] = base_type_id;
    }
    else
    {
        const spirv_size_t constant_id =
            spirv_request_u1_constant (context, (spirv_size_t) type->array_dimensions[start_dimension_index]);

        spirv_size_t *dimension_type_code = spirv_new_instruction (context, &context->higher_type_section, 4u);
        dimension_type_code[0u] |= SpvOpCodeMask & SpvOpTypeArray;
        dimension_type_code[1u] = array_type_id;
        dimension_type_code[2u] = base_type_id;
        dimension_type_code[3u] = constant_id;
    }

    switch (type->class)
    {
    case COMPILER_INSTANCE_TYPE_CLASS_VOID:
    case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
    case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
    case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
    case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
        // No need for array stride for these types.
        break;

    case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
    case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
    case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
    {
        kan_instance_size_t base_size = 0u;
        kan_instance_size_t base_alignment = 0u;
        calculate_type_definition_size_and_alignment (type, start_dimension_index + 1u, &base_size, &base_alignment);

        spirv_size_t *array_stride_code = spirv_new_instruction (context, &context->decoration_section, 4u);
        array_stride_code[0u] |= SpvOpCodeMask & SpvOpDecorate;
        array_stride_code[1u] = array_type_id;
        array_stride_code[2u] = SpvDecorationArrayStride;
        array_stride_code[3u] = (spirv_size_t) base_size;
        break;
    }
    }

    struct spirv_generation_array_type_t *new_array_type =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&context->temporary_allocator, struct spirv_generation_array_type_t);

    new_array_type->next = context->first_generated_array_type;
    context->first_generated_array_type = new_array_type;
    new_array_type->spirv_id = array_type_id;
    copy_type_definition (&new_array_type->type, type);

    if (!new_array_type->type.array_size_runtime)
    {
        new_array_type->type.array_dimensions_count = type->array_dimensions_count - start_dimension_index;
        new_array_type->type.array_dimensions = &type->array_dimensions[start_dimension_index];
    }

    return new_array_type->spirv_id;
}

static inline void spirv_emit_struct_from_declaration_list (struct spirv_generation_context_t *context,
                                                            struct compiler_instance_declaration_node_t *first_field,
                                                            const char *debug_struct_name,
                                                            spirv_size_t struct_id)
{
    spirv_generate_op_name (context, struct_id, debug_struct_name);
    kan_loop_size_t field_count = 0u;
    struct compiler_instance_declaration_node_t *field = first_field;

    while (field)
    {
        ++field_count;
        field = field->next;
    }

    spirv_size_t *fields = NULL;
    if (field_count > 0u)
    {
        fields = kan_stack_group_allocator_allocate (&context->temporary_allocator, sizeof (spirv_size_t) * field_count,
                                                     _Alignof (spirv_size_t));
        kan_loop_size_t field_index = 0u;
        field = first_field;

        while (field)
        {
            spirv_size_t field_type_id = spirv_find_or_generate_object_type (context, &field->variable.type, 0u);
            fields[field_index] = field_type_id;

            spirv_size_t *offset_code = spirv_new_instruction (context, &context->decoration_section, 5u);
            offset_code[0u] |= SpvOpCodeMask & SpvOpMemberDecorate;
            offset_code[1u] = struct_id;
            offset_code[2u] = (spirv_size_t) field_index;
            offset_code[3u] = SpvDecorationOffset;
            offset_code[4u] = (spirv_size_t) field->offset;

            if (field->variable.type.class == COMPILER_INSTANCE_TYPE_CLASS_MATRIX)
            {
                spirv_size_t *column_major_code = spirv_new_instruction (context, &context->decoration_section, 4u);
                column_major_code[0u] |= SpvOpCodeMask & SpvOpMemberDecorate;
                column_major_code[1u] = struct_id;
                column_major_code[2u] = (spirv_size_t) field_index;
                column_major_code[3u] = SpvDecorationColMajor;

                spirv_size_t *matrix_stride_code = spirv_new_instruction (context, &context->decoration_section, 5u);
                matrix_stride_code[0u] |= SpvOpCodeMask & SpvOpMemberDecorate;
                matrix_stride_code[1u] = struct_id;
                matrix_stride_code[2u] = (spirv_size_t) field_index;
                matrix_stride_code[3u] = SpvDecorationMatrixStride;
                matrix_stride_code[4u] = (spirv_size_t) inbuilt_type_item_size[field->variable.type.matrix_data->item] *
                                         field->variable.type.matrix_data->rows;
            }

            spirv_generate_op_member_name (context, struct_id, (spirv_size_t) field_index, field->variable.name);
            field = field->next;
            ++field_index;
        }
    }

    spirv_size_t *struct_code = spirv_new_instruction (context, &context->higher_type_section, 2u + field_count);
    struct_code[0u] |= SpvOpCodeMask & SpvOpTypeStruct;
    struct_code[1u] = struct_id;

    if (fields)
    {
        memcpy (struct_code + 2u, fields, sizeof (spirv_size_t) * field_count);
    }
}

static inline void spirv_emit_location (struct spirv_generation_context_t *context,
                                        spirv_size_t for_id,
                                        kan_rpl_size_t location)
{
    spirv_size_t *location_code = spirv_new_instruction (context, &context->decoration_section, 4u);
    location_code[0u] |= SpvOpCodeMask & SpvOpDecorate;
    location_code[1u] = for_id;
    location_code[2u] = SpvDecorationLocation;
    location_code[3u] = (spirv_size_t) location;
}

static inline void spirv_emit_binding (struct spirv_generation_context_t *context,
                                       spirv_size_t for_id,
                                       kan_rpl_size_t binding)
{
    spirv_size_t *binding_code = spirv_new_instruction (context, &context->decoration_section, 4u);
    binding_code[0u] |= SpvOpCodeMask & SpvOpDecorate;
    binding_code[1u] = for_id;
    binding_code[2u] = SpvDecorationBinding;
    binding_code[3u] = (spirv_size_t) binding;
}

static inline void spirv_emit_descriptor_set (struct spirv_generation_context_t *context,
                                              spirv_size_t for_id,
                                              kan_rpl_size_t descriptor_set)
{
    spirv_size_t *binding_code = spirv_new_instruction (context, &context->decoration_section, 4u);
    binding_code[0u] |= SpvOpCodeMask & SpvOpDecorate;
    binding_code[1u] = for_id;
    binding_code[2u] = SpvDecorationDescriptorSet;
    binding_code[3u] = (spirv_size_t) descriptor_set;
}

static inline void spirv_emit_container_field (struct spirv_generation_context_t *context,
                                               enum kan_rpl_container_type_t container_type,
                                               struct compiler_instance_container_field_node_t *field)
{
    struct compiler_instance_container_field_stage_node_t *stage = field->first_usage_stage;
    while (stage)
    {
        kan_bool_t needs_input = KAN_FALSE;
        kan_bool_t needs_output = KAN_FALSE;
        kan_bool_t make_input_flat_if_integer = KAN_FALSE;

        switch (container_type)
        {
        case KAN_RPL_CONTAINER_TYPE_VERTEX_ATTRIBUTE:
        case KAN_RPL_CONTAINER_TYPE_INSTANCED_ATTRIBUTE:
            // Shouldn't be available outside of vertex stage anyway.
            KAN_ASSERT (stage->user_stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX)
            needs_input = KAN_TRUE;
            needs_output = KAN_FALSE;
            break;

        case KAN_RPL_CONTAINER_TYPE_STATE:
            switch (stage->user_stage)
            {
            case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX:
                needs_input = KAN_FALSE;
                needs_output = KAN_TRUE;
                break;

            case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT:
                needs_input = KAN_TRUE;
                needs_output = KAN_FALSE;
                make_input_flat_if_integer = KAN_TRUE;
                break;
            }

            break;

        case KAN_RPL_CONTAINER_TYPE_COLOR_OUTPUT:
            // Shouldn't be available outside of fragment stage anyway.
            KAN_ASSERT (stage->user_stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT)
            needs_input = KAN_FALSE;
            needs_output = KAN_TRUE;
            break;
        }

        if (needs_input)
        {
            stage->spirv_id_input = context->current_bound;
            ++context->current_bound;

            spirv_size_t *variable_code = spirv_new_instruction (context, &context->global_variable_section, 4u);
            variable_code[0u] |= SpvOpCodeMask & SpvOpVariable;
            variable_code[1u] = spirv_get_or_create_pointer_type (
                context, spirv_find_or_generate_object_type (context, &field->variable.type, 0u), SpvStorageClassInput);

            variable_code[2u] = stage->spirv_id_input;
            variable_code[3u] = SpvStorageClassInput;

            spirv_emit_location (context, stage->spirv_id_input, field->location);
            spirv_generate_op_name (context, stage->spirv_id_input, field->variable.name);

            if (make_input_flat_if_integer)
            {
                enum inbuilt_type_item_t item_type = INBUILT_TYPE_ITEM_FLOAT;
                switch (field->variable.type.class)
                {
                case COMPILER_INSTANCE_TYPE_CLASS_VOID:
                case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
                case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
                case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
                case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
                case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
                    // Unexpected for flattened declarations.
                    KAN_ASSERT (KAN_FALSE)
                    break;

                case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
                    item_type = field->variable.type.vector_data->item;
                    break;

                case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
                    item_type = field->variable.type.matrix_data->item;
                    break;
                }

                switch (item_type)
                {
                case INBUILT_TYPE_ITEM_FLOAT:
                    break;

                case INBUILT_TYPE_ITEM_UNSIGNED:
                case INBUILT_TYPE_ITEM_SIGNED:
                {
                    spirv_size_t *decoration_code = spirv_new_instruction (context, &context->decoration_section, 3u);
                    decoration_code[0u] |= SpvOpCodeMask & SpvOpDecorate;
                    decoration_code[1u] = stage->spirv_id_input;
                    decoration_code[2u] = SpvDecorationFlat;
                    break;
                }
                }
            }
        }
        else
        {
            stage->spirv_id_input = SPIRV_FIXED_ID_INVALID;
        }

        if (needs_output)
        {
            stage->spirv_id_output = context->current_bound;
            ++context->current_bound;

            spirv_size_t *variable_code = spirv_new_instruction (context, &context->global_variable_section, 4u);
            variable_code[0u] |= SpvOpCodeMask & SpvOpVariable;
            variable_code[1u] = spirv_get_or_create_pointer_type (
                context, spirv_find_or_generate_object_type (context, &field->variable.type, 0u),
                SpvStorageClassOutput);

            variable_code[2u] = stage->spirv_id_output;
            variable_code[3u] = SpvStorageClassOutput;

            spirv_emit_location (context, stage->spirv_id_output, field->location);
            spirv_generate_op_name (context, stage->spirv_id_output, field->variable.name);
        }
        else
        {
            stage->spirv_id_output = SPIRV_FIXED_ID_INVALID;
        }

        stage = stage->next;
    }
}

static kan_bool_t spirv_is_uniform_resource_type (struct compiler_instance_type_definition_t *type)
{
    // Returns true if given type can only exist as a uniform resource or pointer to a uniform resource.
    switch (type->class)
    {
    case COMPILER_INSTANCE_TYPE_CLASS_VOID:
    case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
    case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
    case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
    case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
        return KAN_FALSE;

    case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
    case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
    case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
        return KAN_TRUE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static struct spirv_generation_function_type_t *spirv_find_or_generate_function_type (
    struct spirv_generation_context_t *context, struct compiler_instance_function_node_t *function)
{
    const spirv_size_t return_type = spirv_find_or_generate_object_type (context, &function->return_type, 0u);
    kan_loop_size_t argument_count = 0u;
    struct compiler_instance_function_argument_node_t *argument = function->first_argument;

    while (argument)
    {
        ++argument_count;
        argument = argument->next;
    }

    spirv_size_t *argument_types = NULL;
    if (argument_count > 0u)
    {
        argument_types = kan_stack_group_allocator_allocate (
            &context->temporary_allocator, sizeof (spirv_size_t) * argument_count, _Alignof (spirv_size_t));
        kan_loop_size_t argument_index = 0u;
        argument = function->first_argument;

        while (argument)
        {
            // Currently, function arguments are always pointers: we're using the same approach as glslang compiler.
            // The primary reason for that is the fact that this allows us to manage all values in a common and simple
            // manner, as variables basically give as a free pass on ignoring static single-assignment.
            //
            // Perhaps, using more values and less variable pointers might improve compilation or even runtime
            // performance, but checking it is too complex to implement right now and is not a priority due to
            // overload with other various tasks.
            argument_types[argument_index] = spirv_get_or_create_pointer_type (
                context, spirv_find_or_generate_object_type (context, &argument->variable.type, 0u),
                spirv_is_uniform_resource_type (&argument->variable.type) ? SpvStorageClassUniformConstant :
                                                                            SpvStorageClassFunction);
            ++argument_index;
            argument = argument->next;
        }
    }

    struct spirv_generation_function_type_t *function_type = context->first_generated_function_type;
    while (function_type)
    {
        if (function_type->return_type_id == return_type && function_type->argument_count == argument_count &&
            (argument_count == 0u ||
             memcmp (function_type->argument_types, argument_types, argument_count * sizeof (spirv_size_t)) == 0))
        {
            return function_type;
        }

        function_type = function_type->next;
    }

    spirv_size_t function_type_id = context->current_bound;
    ++context->current_bound;

    spirv_size_t *type_code = spirv_new_instruction (context, &context->higher_type_section, 3u + argument_count);
    type_code[0u] |= SpvOpCodeMask & SpvOpTypeFunction;
    type_code[1u] = function_type_id;
    type_code[2u] = return_type;

    if (argument_count > 0u)
    {
        memcpy (type_code + 3u, argument_types, argument_count * sizeof (spirv_size_t));
    }

    struct spirv_generation_function_type_t *new_function_type = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &context->temporary_allocator, struct spirv_generation_function_type_t);

    new_function_type->next = context->first_generated_function_type;
    context->first_generated_function_type = new_function_type;

    new_function_type->generated_id = function_type_id;
    new_function_type->return_type_id = return_type;
    new_function_type->argument_count = argument_count;
    new_function_type->argument_types = argument_types;
    return new_function_type;
}

static inline struct spirv_generation_block_t *spirv_function_new_block (struct spirv_generation_context_t *context,
                                                                         struct spirv_generation_function_node_t *node,
                                                                         spirv_size_t block_id)
{
    struct spirv_generation_block_t *block =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&context->temporary_allocator, struct spirv_generation_block_t);

    block->next = NULL;
    block->spirv_id = block_id;
    block->header_section.first = NULL;
    block->header_section.last = NULL;
    block->code_section.first = NULL;
    block->code_section.last = NULL;
    block->first_persistent_load = NULL;

    spirv_size_t *label_code = spirv_new_instruction (context, &block->header_section, 2u);
    label_code[0u] |= SpvOpCodeMask & SpvOpLabel;
    label_code[1u] = block->spirv_id;

    if (node->last_block)
    {
        node->last_block->next = block;
    }
    else
    {
        node->first_block = block;
    }

    node->last_block = block;
    return block;
}

static inline void spirv_add_persistent_load (struct spirv_generation_context_t *context,
                                              struct spirv_generation_block_t *block,
                                              spirv_size_t variable_id,
                                              spirv_size_t token_id)
{
    struct spirv_block_persistent_load_t *persistent_load =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&context->temporary_allocator, struct spirv_block_persistent_load_t);

    persistent_load->next = block->first_persistent_load;
    block->first_persistent_load = persistent_load;
    persistent_load->variable_id = variable_id;
    persistent_load->token_id = token_id;
}

static spirv_size_t spirv_request_load (struct spirv_generation_context_t *context,
                                        struct spirv_generation_block_t *load_block,
                                        spirv_size_t type_id,
                                        spirv_size_t variable_id,
                                        kan_bool_t persistent_load_allowed)
{
    if (persistent_load_allowed)
    {
        // If we've already loaded variable in this block and didn't store anything to it, we can continue using it.
        struct spirv_block_persistent_load_t *persistent_load = load_block->first_persistent_load;

        while (persistent_load)
        {
            if (persistent_load->variable_id == variable_id)
            {
                return persistent_load->token_id;
            }

            persistent_load = persistent_load->next;
        }
    }

    spirv_size_t loaded_id = context->current_bound;
    ++context->current_bound;

    spirv_size_t *load_code = spirv_new_instruction (context, &load_block->code_section, 4u);
    load_code[0u] |= SpvOpCodeMask & SpvOpLoad;
    load_code[1u] = type_id;
    load_code[2u] = loaded_id;
    load_code[3u] = variable_id;

    if (persistent_load_allowed)
    {
        // Record persistent load to avoid duplications.
        spirv_add_persistent_load (context, load_block, variable_id, loaded_id);
    }

    return loaded_id;
}

static void spirv_emit_store (struct spirv_generation_context_t *context,
                              struct spirv_generation_block_t *store_block,
                              spirv_size_t variable_id,
                              spirv_size_t data_id)
{
    spirv_size_t *load_code = spirv_new_instruction (context, &store_block->code_section, 3u);
    load_code[0u] |= SpvOpCodeMask & SpvOpStore;
    load_code[1u] = variable_id;
    load_code[2u] = data_id;

    // Replace persistent load with the new value if it exists.
    struct spirv_block_persistent_load_t *persistent_load = store_block->first_persistent_load;

    while (persistent_load)
    {
        if (persistent_load->variable_id == variable_id)
        {
            persistent_load->token_id = data_id;
            return;
        }

        persistent_load = persistent_load->next;
    }

    // No old persistent load, add new one.
    spirv_add_persistent_load (context, store_block, variable_id, data_id);
}

static inline void spirv_register_builtin_usage (struct spirv_generation_context_t *context,
                                                 struct spirv_generation_function_node_t *function,
                                                 struct spirv_generation_builtin_t *builtin)
{
    struct spirv_generation_builtin_used_by_stage_t *stage_usage = builtin->first_stage;
    while (stage_usage)
    {
        if (stage_usage->stage == function->source->required_stage)
        {
            return;
        }

        stage_usage = stage_usage->next;
    }

    stage_usage = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&context->temporary_allocator,
                                                            struct spirv_generation_builtin_used_by_stage_t);
    stage_usage->next = builtin->first_stage;
    builtin->first_stage = stage_usage;
    stage_usage->stage = function->source->required_stage;
}

static spirv_size_t spirv_request_builtin (struct spirv_generation_context_t *context,
                                           struct spirv_generation_function_node_t *associated_function,
                                           SpvBuiltIn builtin_type,
                                           SpvStorageClass builtin_storage,
                                           spirv_size_t builtin_variable_type)
{
    struct spirv_generation_builtin_t *existent_builtin = context->first_builtin;
    while (existent_builtin)
    {
        if (existent_builtin->builtin_type == builtin_type && existent_builtin->builtin_storage == builtin_storage)
        {
            spirv_register_builtin_usage (context, associated_function, existent_builtin);
            return existent_builtin->spirv_id;
        }

        existent_builtin = existent_builtin->next;
    }

    spirv_size_t variable_id = context->current_bound;
    ++context->current_bound;

    struct spirv_generation_builtin_t *new_builtin =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&context->temporary_allocator, struct spirv_generation_builtin_t);
    new_builtin->next = context->first_builtin;
    context->first_builtin = new_builtin;
    new_builtin->spirv_id = variable_id;
    new_builtin->builtin_type = builtin_type;
    new_builtin->builtin_storage = builtin_storage;
    new_builtin->first_stage = NULL;

    spirv_size_t *decoration_code = spirv_new_instruction (context, &context->decoration_section, 4u);
    decoration_code[0u] |= SpvOpCodeMask & SpvOpDecorate;
    decoration_code[1u] = variable_id;
    decoration_code[2u] = SpvDecorationBuiltIn;
    decoration_code[3u] = builtin_type;

    spirv_size_t *variable_code = spirv_new_instruction (context, &context->global_variable_section, 4u);
    variable_code[0u] |= SpvOpCodeMask & SpvOpVariable;
    variable_code[1u] = builtin_variable_type;
    variable_code[2u] = variable_id;
    variable_code[3u] = builtin_storage;

    spirv_register_builtin_usage (context, associated_function, new_builtin);
    return variable_id;
}

static kan_instance_size_t spirv_count_access_chain_elements (
    struct compiler_instance_expression_node_t *top_expression,
    kan_bool_t *can_be_out_of_bounds,
    struct compiler_instance_expression_node_t **root_expression)
{
    if (top_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS)
    {
        return top_expression->structured_access.access_chain_length +
               spirv_count_access_chain_elements (top_expression->structured_access.input, can_be_out_of_bounds,
                                                  root_expression);
    }
    else if (top_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ARRAY_INDEX)
    {
        *can_be_out_of_bounds = KAN_TRUE;
        return 1u + spirv_count_access_chain_elements (top_expression->binary_operation.left_operand,
                                                       can_be_out_of_bounds, root_expression);
    }
    // Instanced buffers generate hidden instance data access.
    else if (top_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE)
    {
        *root_expression = top_expression;
        switch (top_expression->structured_buffer_reference->type)
        {
        case KAN_RPL_BUFFER_TYPE_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
        case KAN_RPL_BUFFER_TYPE_PUSH_CONSTANT:
            return 0u;
        }
    }

    *root_expression = top_expression;
    return 0u;
}

static spirv_size_t spirv_emit_expression (struct spirv_generation_context_t *context,
                                           struct spirv_generation_function_node_t *function,
                                           struct spirv_generation_block_t **current_block,
                                           struct compiler_instance_expression_node_t *expression,
                                           kan_bool_t result_should_be_pointer);

static spirv_size_t *spirv_fill_access_chain_elements (struct spirv_generation_context_t *context,
                                                       struct spirv_generation_function_node_t *function,
                                                       struct spirv_generation_block_t **current_block,
                                                       struct compiler_instance_expression_node_t *top_expression,
                                                       spirv_size_t *output)
{
    if (top_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS)
    {
        output = spirv_fill_access_chain_elements (context, function, current_block,
                                                   top_expression->structured_access.input, output);

        for (kan_loop_size_t index = 0u; index < top_expression->structured_access.access_chain_length; ++index)
        {
            KAN_ASSERT (top_expression->structured_access.access_chain_indices[index] < INT32_MAX)
            spirv_size_t constant_id = spirv_request_u1_constant (
                context, (spirv_unsigned_literal_t) top_expression->structured_access.access_chain_indices[index]);

            *output = constant_id;
            ++output;
        }

        return output;
    }
    else if (top_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ARRAY_INDEX)
    {
        output = spirv_fill_access_chain_elements (context, function, current_block,
                                                   top_expression->binary_operation.left_operand, output);

        spirv_size_t index_id = spirv_emit_expression (context, function, current_block,
                                                       top_expression->binary_operation.right_operand, KAN_FALSE);

        // Currently, we treat all image indexing operations as non-uniform for safety.
        // It makes performance a little bit worse, we might improve how it is handled later.
        if (top_expression->binary_operation.left_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_IMAGE &&
            // If image is accessed through literal, access can never be non-uniform.
            top_expression->binary_operation.right_operand->type != COMPILER_INSTANCE_EXPRESSION_TYPE_UNSIGNED_LITERAL)
        {
            context->extension_requirement_non_uniform = KAN_TRUE;
            spirv_size_t *non_uniform_decoration = spirv_new_instruction (context, &context->decoration_section, 3u);
            non_uniform_decoration[0u] |= SpvOpCodeMask & SpvOpDecorate;
            non_uniform_decoration[1u] = index_id;
            non_uniform_decoration[2u] = SpvDecorationNonUniformEXT;
        }

        *output = index_id;
        ++output;
        return output;
    }
    else if (top_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE)
    {
        switch (top_expression->structured_buffer_reference->type)
        {
        case KAN_RPL_BUFFER_TYPE_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
        case KAN_RPL_BUFFER_TYPE_PUSH_CONSTANT:
            return output;
        }
    }

    return output;
}

static inline SpvStorageClass spirv_get_structured_buffer_storage_class (struct compiler_instance_buffer_node_t *buffer)
{
    switch (buffer->type)
    {
    case KAN_RPL_BUFFER_TYPE_UNIFORM:
        return SpvStorageClassUniform;

    case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
        return SpvStorageClassStorageBuffer;

    case KAN_RPL_BUFFER_TYPE_PUSH_CONSTANT:
        return SpvStorageClassPushConstant;
    }

    KAN_ASSERT (KAN_FALSE)
    return (SpvStorageClass) SPIRV_FIXED_ID_INVALID;
}

static spirv_size_t spirv_use_temporary_variable (struct spirv_generation_context_t *context,
                                                  struct spirv_generation_function_node_t *function,
                                                  struct compiler_instance_type_definition_t *required_type)
{
    // We do not expect temporary variables to be arrays or booleans.
    // If they are, then something is wrong with the resolve.
    KAN_ASSERT (required_type->array_dimensions_count == 0u)
    KAN_ASSERT (!required_type->array_size_runtime)

    spirv_size_t required_type_id = SPIRV_FIXED_ID_INVALID;
    switch (required_type->class)
    {
    case COMPILER_INSTANCE_TYPE_CLASS_VOID:
    case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
    case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
    case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
    case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
        // Not expected in this context.
        KAN_ASSERT (KAN_FALSE)
        break;

    case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
    case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
    case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
        required_type_id = spirv_find_or_generate_object_type (context, required_type, 0u);
        break;
    }

    required_type_id = spirv_get_or_create_pointer_type (context, required_type_id, SpvStorageClassFunction);
    struct spirv_generation_temporary_variable_t *previous_free_variable = NULL;
    struct spirv_generation_temporary_variable_t *free_variable = function->first_free_temporary_variable;

    while (free_variable)
    {
        if (free_variable->spirv_type_id == required_type_id)
        {
            if (previous_free_variable)
            {
                previous_free_variable->next = free_variable->next;
            }
            else
            {
                function->first_free_temporary_variable = free_variable->next;
            }

            free_variable->next = function->first_used_temporary_variable;
            function->first_used_temporary_variable = free_variable;
            return free_variable->spirv_id;
        }

        previous_free_variable = free_variable;
        free_variable = free_variable->next;
    }

    struct spirv_generation_temporary_variable_t *new_variable = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &context->temporary_allocator, struct spirv_generation_temporary_variable_t);

    new_variable->next = function->first_used_temporary_variable;
    function->first_used_temporary_variable = new_variable;
    new_variable->spirv_id = context->current_bound;
    ++context->current_bound;
    new_variable->spirv_type_id = required_type_id;

    spirv_size_t *variable_code = spirv_new_instruction (context, &function->first_block->header_section, 4u);
    variable_code[0u] |= SpvOpCodeMask & SpvOpVariable;
    variable_code[1u] = required_type_id;
    variable_code[2u] = new_variable->spirv_id;
    variable_code[3u] = SpvStorageClassFunction;

    return new_variable->spirv_id;
}

static inline spirv_size_t spirv_emit_access_chain (struct spirv_generation_context_t *context,
                                                    struct spirv_generation_function_node_t *function,
                                                    struct spirv_generation_block_t **current_block,
                                                    struct compiler_instance_expression_node_t *top_expression,
                                                    kan_bool_t result_should_be_pointer)
{
    // Only can be out of bounds if we're indexing arrays.
    kan_bool_t can_be_out_of_bounds = KAN_FALSE;
    struct compiler_instance_expression_node_t *root_expression = NULL;
    kan_instance_size_t access_chain_length =
        spirv_count_access_chain_elements (top_expression, &can_be_out_of_bounds, &root_expression);
    KAN_ASSERT (access_chain_length > 0u)
    KAN_ASSERT (root_expression)

    spirv_size_t *access_chain_elements = kan_stack_group_allocator_allocate (
        &context->temporary_allocator, sizeof (spirv_size_t) * access_chain_length, _Alignof (spirv_size_t));
    spirv_fill_access_chain_elements (context, function, current_block, top_expression, access_chain_elements);

    spirv_size_t base_id = spirv_emit_expression (context, function, current_block, root_expression, KAN_TRUE);
    spirv_size_t result_id = context->current_bound;
    ++context->current_bound;

    spirv_size_t result_value_type = spirv_find_or_generate_object_type (context, &top_expression->output, 0u);
    spirv_size_t result_pointer_type = (spirv_size_t) SPIRV_FIXED_ID_INVALID;

    switch (root_expression->type)
    {
    case COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE:
        result_pointer_type = spirv_get_or_create_pointer_type (
            context, result_value_type,
            spirv_get_structured_buffer_storage_class (root_expression->structured_buffer_reference));
        break;

    case COMPILER_INSTANCE_EXPRESSION_TYPE_SAMPLER_REFERENCE:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ARRAY_INDEX:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_FLOATING_LITERAL:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_UNSIGNED_LITERAL:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_SIGNED_LITERAL:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_SCOPE:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_IF:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_FOR:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_WHILE:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_BREAK:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_CONTINUE:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_RETURN:
        KAN_ASSERT (KAN_FALSE)
        result_pointer_type = (spirv_size_t) SPIRV_FIXED_ID_INVALID;
        break;

    case COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_REFERENCE:
        result_pointer_type =
            spirv_get_or_create_pointer_type (context, result_value_type, SpvStorageClassUniformConstant);
        break;

    case COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_REFERENCE:
    {
        // If variable internals are accessed through pointer, its persistent load is compromised and should be deleted.
        struct spirv_block_persistent_load_t *persistent_load = (*current_block)->first_persistent_load;
        struct spirv_block_persistent_load_t *previous = NULL;

        while (persistent_load)
        {
            if (persistent_load->variable_id == root_expression->variable_reference->spirv_id)
            {
                if (previous)
                {
                    previous->next = persistent_load->next;
                }
                else
                {
                    (*current_block)->first_persistent_load = persistent_load->next;
                }

                break;
            }

            previous = persistent_load;
            persistent_load = persistent_load->next;
        }

        result_pointer_type = spirv_get_or_create_pointer_type (context, result_value_type, SpvStorageClassFunction);
        break;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_SWIZZLE:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_DECLARATION:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ADD:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_SUBTRACT:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_MULTIPLY:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_DIVIDE:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_MODULUS:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ASSIGN:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_AND:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_OR:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_EQUAL:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NOT_EQUAL:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_LESS:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_GREATER:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_LESS_OR_EQUAL:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_GREATER_OR_EQUAL:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_AND:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_OR:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_XOR:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_LEFT_SHIFT:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_RIGHT_SHIFT:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NEGATE:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NOT:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_NOT:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_FUNCTION_CALL:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_SAMPLE:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_SAMPLE_DREF:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_VECTOR_CONSTRUCTOR:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_MATRIX_CONSTRUCTOR:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCT_CONSTRUCTOR:
        KAN_ASSERT (!spirv_is_uniform_resource_type (&top_expression->output))
        result_pointer_type = spirv_get_or_create_pointer_type (context, result_value_type, SpvStorageClassFunction);
        break;

    case COMPILER_INSTANCE_EXPRESSION_TYPE_CONTAINER_FIELD_ACCESS_INPUT:
        result_pointer_type = spirv_get_or_create_pointer_type (context, result_value_type, SpvStorageClassInput);
        break;

    case COMPILER_INSTANCE_EXPRESSION_TYPE_CONTAINER_FIELD_ACCESS_OUTPUT:
        result_pointer_type = spirv_get_or_create_pointer_type (context, result_value_type, SpvStorageClassOutput);
        break;
    }

    spirv_size_t *access_chain_code =
        spirv_new_instruction (context, &(*current_block)->code_section, 4u + access_chain_length);
    access_chain_code[0u] |= SpvOpCodeMask & (can_be_out_of_bounds ? SpvOpAccessChain : SpvOpInBoundsAccessChain);
    access_chain_code[1u] = result_pointer_type;
    access_chain_code[2u] = result_id;
    access_chain_code[3u] = base_id;
    memcpy (access_chain_code + 4u, access_chain_elements, sizeof (spirv_size_t) * access_chain_length);

    if (!result_should_be_pointer)
    {
        // Currently we forbid live load for access chain results as we have no way to track
        // when persistent load is no longer correct.
        result_id = spirv_request_load (context, *current_block, result_value_type, result_id, KAN_FALSE);
    }

    return result_id;
}

static inline spirv_size_t spirv_emit_single_composite_extract (struct spirv_generation_context_t *context,
                                                                struct spirv_arbitrary_instruction_section_t *section,
                                                                spirv_size_t result_type_id,
                                                                spirv_size_t object_id,
                                                                spirv_size_t composite_id)
{
    spirv_size_t result_id = context->current_bound;
    ++context->current_bound;

    spirv_size_t *code = spirv_new_instruction (context, section, 5u);
    code[0u] |= SpvOpCodeMask & SpvOpCompositeExtract;
    code[1u] = result_type_id;
    code[2u] = result_id;
    code[3u] = object_id;
    code[4u] = composite_id;

    return result_id;
}

#define SPIRV_EMIT_CONVERT(FUNCTION_SUFFIX, OPERATION)                                                                 \
    static inline spirv_size_t spirv_emit_convert_##FUNCTION_SUFFIX (                                                  \
        struct spirv_generation_context_t *context, struct spirv_arbitrary_instruction_section_t *section,             \
        spirv_size_t result_type_id, spirv_size_t object_id)                                                           \
    {                                                                                                                  \
        spirv_size_t result_id = context->current_bound;                                                               \
        ++context->current_bound;                                                                                      \
                                                                                                                       \
        spirv_size_t *code = spirv_new_instruction (context, section, 4u);                                             \
        code[0u] |= SpvOpCodeMask & OPERATION;                                                                         \
        code[1u] = result_type_id;                                                                                     \
        code[2u] = result_id;                                                                                          \
        code[3u] = object_id;                                                                                          \
                                                                                                                       \
        return result_id;                                                                                              \
    }

SPIRV_EMIT_CONVERT (signed_to_float, SpvOpConvertSToF)
SPIRV_EMIT_CONVERT (float_to_signed, SpvOpConvertFToS)
SPIRV_EMIT_CONVERT (unsigned_to_float, SpvOpConvertUToF)
SPIRV_EMIT_CONVERT (float_to_unsigned, SpvOpConvertFToU)
SPIRV_EMIT_CONVERT (signed_to_unsigned, SpvOpBitcast)
SPIRV_EMIT_CONVERT (unsigned_to_signed, SpvOpBitcast)
#undef SPIRV_EMIT_CONVERT

static inline spirv_size_t spirv_convert_vector (struct spirv_generation_context_t *context,
                                                 struct spirv_arbitrary_instruction_section_t *section,
                                                 enum inbuilt_type_item_t result_item,
                                                 enum inbuilt_type_item_t source_item,
                                                 spirv_size_t operand_id,
                                                 spirv_size_t result_type_id)
{
    switch (result_item)
    {
    case INBUILT_TYPE_ITEM_FLOAT:
        switch (source_item)
        {
        case INBUILT_TYPE_ITEM_FLOAT:
            return operand_id;

        case INBUILT_TYPE_ITEM_UNSIGNED:
            return spirv_emit_convert_unsigned_to_float (context, section, result_type_id, operand_id);

        case INBUILT_TYPE_ITEM_SIGNED:
            return spirv_emit_convert_signed_to_float (context, section, result_type_id, operand_id);
        }

        break;

    case INBUILT_TYPE_ITEM_UNSIGNED:
        switch (source_item)
        {
        case INBUILT_TYPE_ITEM_FLOAT:
            return spirv_emit_convert_float_to_unsigned (context, section, result_type_id, operand_id);

        case INBUILT_TYPE_ITEM_UNSIGNED:
            return operand_id;

        case INBUILT_TYPE_ITEM_SIGNED:
            return spirv_emit_convert_signed_to_unsigned (context, section, result_type_id, operand_id);
        }

        break;

    case INBUILT_TYPE_ITEM_SIGNED:
        switch (source_item)
        {
        case INBUILT_TYPE_ITEM_FLOAT:
            return spirv_emit_convert_float_to_signed (context, section, result_type_id, operand_id);

        case INBUILT_TYPE_ITEM_UNSIGNED:
            return spirv_emit_convert_unsigned_to_signed (context, section, result_type_id, operand_id);

        case INBUILT_TYPE_ITEM_SIGNED:
            return operand_id;
        }

        break;
    }

    KAN_ASSERT (KAN_FALSE)
    return SPIRV_FIXED_ID_INVALID;
}

#define SPIRV_EMIT_VECTOR_ARITHMETIC(SUFFIX, FLOAT_OP, UNSIGNED_OP, SIGNED_OP)                                         \
    static inline spirv_size_t spirv_emit_vector_##SUFFIX (                                                            \
        struct spirv_generation_context_t *context, struct spirv_arbitrary_instruction_section_t *section,             \
        struct inbuilt_vector_type_t *type, spirv_size_t left, spirv_size_t right)                                     \
    {                                                                                                                  \
        spirv_size_t result_id = context->current_bound;                                                               \
        ++context->current_bound;                                                                                      \
                                                                                                                       \
        switch (type->item)                                                                                            \
        {                                                                                                              \
        case INBUILT_TYPE_ITEM_FLOAT:                                                                                  \
        {                                                                                                              \
            spirv_size_t *code = spirv_new_instruction (context, section, 5u);                                         \
            code[0u] |= SpvOpCodeMask & FLOAT_OP;                                                                      \
            code[1u] = spirv_find_or_generate_vector_type (context, (kan_rpl_size_t) (type - STATICS.vector_types));   \
            code[2u] = result_id;                                                                                      \
            code[3u] = left;                                                                                           \
            code[4u] = right;                                                                                          \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        case INBUILT_TYPE_ITEM_UNSIGNED:                                                                               \
        {                                                                                                              \
            spirv_size_t *code = spirv_new_instruction (context, section, 5u);                                         \
            code[0u] |= SpvOpCodeMask & UNSIGNED_OP;                                                                   \
            code[1u] = spirv_find_or_generate_vector_type (context, (kan_rpl_size_t) (type - STATICS.vector_types));   \
            code[2u] = result_id;                                                                                      \
            code[3u] = left;                                                                                           \
            code[4u] = right;                                                                                          \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        case INBUILT_TYPE_ITEM_SIGNED:                                                                                 \
        {                                                                                                              \
            spirv_size_t *code = spirv_new_instruction (context, section, 5u);                                         \
            code[0u] |= SpvOpCodeMask & SIGNED_OP;                                                                     \
            code[1u] = spirv_find_or_generate_vector_type (context, (kan_rpl_size_t) (type - STATICS.vector_types));   \
            code[2u] = result_id;                                                                                      \
            code[3u] = left;                                                                                           \
            code[4u] = right;                                                                                          \
            break;                                                                                                     \
        }                                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        return result_id;                                                                                              \
    }

SPIRV_EMIT_VECTOR_ARITHMETIC (add, SpvOpFAdd, SpvOpIAdd, SpvOpIAdd)
SPIRV_EMIT_VECTOR_ARITHMETIC (sub, SpvOpFSub, SpvOpISub, SpvOpISub)
SPIRV_EMIT_VECTOR_ARITHMETIC (mul, SpvOpFMul, SpvOpIMul, SpvOpIMul)
SPIRV_EMIT_VECTOR_ARITHMETIC (div, SpvOpFDiv, SpvOpUDiv, SpvOpSDiv)
#undef SPIRV_EMIT_VECTOR_ARITHMETIC

#define SPIRV_EMIT_MATRIX_ARITHMETIC(SUFFIX)                                                                           \
    static inline spirv_size_t spirv_emit_matrix_##SUFFIX (                                                            \
        struct spirv_generation_context_t *context, struct spirv_arbitrary_instruction_section_t *section,             \
        struct inbuilt_matrix_type_t *type, spirv_size_t left, spirv_size_t right)                                     \
    {                                                                                                                  \
        spirv_size_t column_result_ids[4u];                                                                            \
        KAN_ASSERT (type->columns <= 4u)                                                                               \
        spirv_size_t column_type_index = INBUILT_VECTOR_TYPE_INDEX (type->item, type->rows);                           \
        spirv_size_t column_type_id = spirv_find_or_generate_vector_type (context, column_type_index);                 \
        struct inbuilt_vector_type_t *column_type = &STATICS.vector_types[column_type_index];                          \
                                                                                                                       \
        for (kan_loop_size_t column_index = 0u; column_index < type->columns; ++column_index)                          \
        {                                                                                                              \
            spirv_size_t left_extract_result = spirv_emit_single_composite_extract (                                   \
                context, section, column_type_id, left, (spirv_size_t) column_index);                                  \
                                                                                                                       \
            spirv_size_t right_extract_result = spirv_emit_single_composite_extract (                                  \
                context, section, column_type_id, right, (spirv_size_t) column_index);                                 \
                                                                                                                       \
            column_result_ids[column_index] =                                                                          \
                spirv_emit_vector_##SUFFIX (context, section, column_type, left_extract_result, right_extract_result); \
        }                                                                                                              \
                                                                                                                       \
        spirv_size_t result_id = context->current_bound;                                                               \
        ++context->current_bound;                                                                                      \
                                                                                                                       \
        spirv_size_t *construct = spirv_new_instruction (context, section, 3u + type->columns);                        \
        construct[0u] |= SpvOpCodeMask & SpvOpCompositeConstruct;                                                      \
        construct[1u] = spirv_find_or_generate_matrix_type (context, (kan_rpl_size_t) (type - STATICS.matrix_types));  \
        construct[2u] = result_id;                                                                                     \
        memcpy (construct + 3u, column_result_ids, type->columns * sizeof (spirv_size_t));                             \
        return result_id;                                                                                              \
    }

SPIRV_EMIT_MATRIX_ARITHMETIC (add)
SPIRV_EMIT_MATRIX_ARITHMETIC (sub)
SPIRV_EMIT_MATRIX_ARITHMETIC (div)
#undef SPIRV_EMIT_MATRIX_ARITHMETIC

static void spirv_if_temporary_variable_then_stop_using (struct spirv_generation_function_node_t *function,
                                                         spirv_size_t spirv_id)
{
    struct spirv_generation_temporary_variable_t *previous_used_variable = NULL;
    struct spirv_generation_temporary_variable_t *used_variable = function->first_used_temporary_variable;

    while (used_variable)
    {
        if (used_variable->spirv_id == spirv_id)
        {
            if (previous_used_variable)
            {
                previous_used_variable->next = used_variable->next;
            }
            else
            {
                function->first_used_temporary_variable = used_variable->next;
            }

            used_variable->next = function->first_free_temporary_variable;
            function->first_free_temporary_variable = used_variable;
            return;
        }

        previous_used_variable = used_variable;
        used_variable = used_variable->next;
    }
}

static inline spirv_size_t *spirv_gather_call_arguments (
    struct spirv_generation_context_t *context,
    struct spirv_generation_function_node_t *function,
    struct spirv_generation_block_t **current_block,
    struct compiler_instance_expression_list_item_t *first_argument,
    kan_instance_size_t *argument_count,
    kan_bool_t arguments_should_be_pointers)
{
    *argument_count = 0u;
    struct compiler_instance_expression_list_item_t *argument = first_argument;

    while (argument)
    {
        ++*argument_count;
        argument = argument->next;
    }

    spirv_size_t *arguments = NULL;
    if (*argument_count > 0u)
    {
        arguments = kan_stack_group_allocator_allocate (
            &context->temporary_allocator, sizeof (spirv_size_t) * *argument_count, _Alignof (spirv_size_t));
        kan_loop_size_t argument_index = 0u;
        argument = first_argument;

        while (argument)
        {
            arguments[argument_index] = spirv_emit_expression (context, function, current_block, argument->expression,
                                                               arguments_should_be_pointers);

            kan_bool_t pointer_argument_needs_to_be_interned =
                arguments_should_be_pointers && !spirv_is_uniform_resource_type (&argument->expression->output) &&
                // Special case for flattened input variables: they're technically pointers, but they have different
                // storage type, therefore they should be copied into function variable first.
                (argument->expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_CONTAINER_FIELD_ACCESS_INPUT ||
                 // Special case for access chains: their results can not be passed as function arguments as
                 // requested by SPIRV specification, therefore they should be copied into function variable first.
                 argument->expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS ||
                 argument->expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ARRAY_INDEX);

            if (pointer_argument_needs_to_be_interned)
            {
                spirv_size_t variable_id =
                    spirv_use_temporary_variable (context, function, &argument->expression->output);

                spirv_size_t *copy_code = spirv_new_instruction (context, &(*current_block)->code_section, 3u);
                copy_code[0u] |= SpvOpCodeMask & SpvOpCopyMemory;
                copy_code[1u] = variable_id;
                copy_code[2u] = arguments[argument_index];
                arguments[argument_index] = variable_id;
            }

            ++argument_index;
            argument = argument->next;
        }
    }

    return arguments;
}

static inline spirv_size_t spirv_emit_extension_instruction (struct spirv_generation_context_t *context,
                                                             struct spirv_generation_function_node_t *function,
                                                             struct spirv_generation_block_t **current_block,
                                                             struct compiler_instance_expression_node_t *expression,
                                                             spirv_size_t library,
                                                             spirv_size_t extension)
{
    kan_instance_size_t argument_count = 0u;
    spirv_size_t *arguments = spirv_gather_call_arguments (
        context, function, current_block, expression->function_call.first_argument, &argument_count, KAN_FALSE);

    spirv_size_t result_id = context->current_bound;
    ++context->current_bound;

    const spirv_size_t result_type_id =
        spirv_find_or_generate_object_type (context, &expression->function_call.function->return_type, 0u);

    spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 5u + argument_count);
    code[0u] |= SpvOpCodeMask & SpvOpExtInst;
    code[1u] = result_type_id;
    code[2u] = result_id;
    code[3u] = library;
    code[4u] = extension;

    if (arguments)
    {
        memcpy (code + 5u, arguments, sizeof (spirv_size_t) * argument_count);
    }

    return result_id;
}

static spirv_size_t spirv_emit_inbuilt_function_call (struct spirv_generation_context_t *context,
                                                      struct spirv_generation_function_node_t *function,
                                                      struct spirv_generation_block_t **current_block,
                                                      struct compiler_instance_expression_node_t *expression)
{
    if (expression->function_call.function->spirv_external_library_id != (spirv_size_t) SPIRV_FIXED_ID_INVALID)
    {
        return spirv_emit_extension_instruction (context, function, current_block, expression,
                                                 expression->function_call.function->spirv_external_library_id,
                                                 expression->function_call.function->spirv_external_instruction_id);
    }
    else if (expression->function_call.function == &STATICS.builtin_vertex_stage_output_position)
    {
        spirv_size_t operand_id = spirv_emit_expression (
            context, function, current_block, expression->function_call.first_argument->expression, KAN_FALSE);

        spirv_size_t type_id =
            spirv_find_or_generate_vector_type (context, INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_FLOAT, 4u));
        spirv_size_t pointer_type_id = spirv_get_or_create_pointer_type (context, type_id, SpvStorageClassOutput);

        const spirv_size_t position_builtin =
            spirv_request_builtin (context, function, SpvBuiltInPosition, SpvStorageClassOutput, pointer_type_id);

        spirv_emit_store (context, *current_block, position_builtin, operand_id);
        // Just a store operation, has no return.
        return (spirv_size_t) SPIRV_FIXED_ID_INVALID;
    }
    else if (expression->function_call.function == &STATICS.builtin_fragment_stage_discard)
    {
        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 1u);
        code[0u] |= SpvOpCodeMask & SpvOpKill;
        // Just a termination operation, has no return.
        return (spirv_size_t) SPIRV_FIXED_ID_INVALID;
    }
    else if (expression->function_call.function == &STATICS.builtin_pi)
    {
        return spirv_request_f1_constant (context, (float) M_PI);
    }
    else if (expression->function_call.function == &STATICS.builtin_transpose_matrix_f3x3 ||
             expression->function_call.function == &STATICS.builtin_transpose_matrix_f4x4)
    {
        spirv_size_t operand_id = spirv_emit_expression (
            context, function, current_block, expression->function_call.first_argument->expression, KAN_FALSE);

        spirv_size_t result_id = context->current_bound;
        ++context->current_bound;

        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 4u);
        code[0u] |= SpvOpCodeMask & SpvOpTranspose;
        code[1u] = spirv_find_or_generate_matrix_type (
            context,
            (kan_rpl_size_t) (expression->function_call.function->return_type.matrix_data - STATICS.matrix_types));
        code[2u] = result_id;
        code[3u] = operand_id;

        return result_id;
    }
    else if (expression->function_call.function == &STATICS.builtin_dot_f2 ||
             expression->function_call.function == &STATICS.builtin_dot_f3 ||
             expression->function_call.function == &STATICS.builtin_dot_f4)
    {
        struct compiler_instance_expression_list_item_t *left_argument = expression->function_call.first_argument;
        struct compiler_instance_expression_list_item_t *right_argument = left_argument->next;

        spirv_size_t left_operand_id =
            spirv_emit_expression (context, function, current_block, left_argument->expression, KAN_FALSE);
        spirv_size_t right_operand_id =
            spirv_emit_expression (context, function, current_block, right_argument->expression, KAN_FALSE);

        spirv_size_t result_id = context->current_bound;
        ++context->current_bound;

        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
        code[0u] |= SpvOpCodeMask & SpvOpDot;
        code[1u] = spirv_find_or_generate_vector_type (
            context,
            (kan_rpl_size_t) (expression->function_call.function->return_type.vector_data - STATICS.vector_types));
        code[2u] = result_id;
        code[3u] = left_operand_id;
        code[4u] = right_operand_id;

        return result_id;
    }

    // Unknown inbuilt function, how this happened?
    KAN_ASSERT (KAN_FALSE)
    return (spirv_size_t) SPIRV_FIXED_ID_INVALID;
}

static spirv_size_t spirv_emit_expression (struct spirv_generation_context_t *context,
                                           struct spirv_generation_function_node_t *function,
                                           struct spirv_generation_block_t **current_block,
                                           struct compiler_instance_expression_node_t *expression,
                                           kan_bool_t result_should_be_pointer)
{
    switch (expression->type)
    {
    case COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE:
        // If buffer reference result is requested as not pointer, then something is off with resolve or AST.
        KAN_ASSERT (result_should_be_pointer)
        return expression->structured_buffer_reference->structured_variable_spirv_id;

    case COMPILER_INSTANCE_EXPRESSION_TYPE_SAMPLER_REFERENCE:
        if (result_should_be_pointer)
        {
            return expression->sampler_reference->variable_spirv_id;
        }

        return spirv_request_load (context, *current_block, spirv_find_or_generate_sampler_type (context),
                                   expression->sampler_reference->variable_spirv_id, KAN_TRUE);

    case COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_REFERENCE:
        if (result_should_be_pointer)
        {
            return expression->image_reference->variable_spirv_id;
        }

        return spirv_request_load (context, *current_block,
                                   spirv_find_or_generate_image_type (context, expression->image_reference->type).image,
                                   expression->image_reference->variable_spirv_id, KAN_TRUE);

    case COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_REFERENCE:
        if (result_should_be_pointer)
        {
            return expression->variable_reference->spirv_id;
        }

        return spirv_request_load (context, *current_block,
                                   spirv_find_or_generate_object_type (context, &expression->output, 0u),
                                   expression->variable_reference->spirv_id, KAN_TRUE);

    case COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS:
        // Sometimes access chains can be replaced by composite extracts (for better performance),
        // but we do not support it right now.
        return spirv_emit_access_chain (context, function, current_block, expression, result_should_be_pointer);

#define WRAP_OPERATION_RESULT_IF_NEEDED                                                                                \
    if (result_should_be_pointer)                                                                                      \
    {                                                                                                                  \
        spirv_size_t variable_id = spirv_use_temporary_variable (context, function, &expression->output);              \
        spirv_emit_store (context, *current_block, variable_id, result_id);                                            \
        result_id = variable_id;                                                                                       \
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_SWIZZLE:
    {
        spirv_size_t operand_id =
            spirv_emit_expression (context, function, current_block, expression->swizzle.input, KAN_FALSE);

        spirv_size_t result_type_id = spirv_find_or_generate_vector_type (
            context, (kan_rpl_size_t) (expression->output.vector_data - STATICS.vector_types));

        spirv_size_t result_id = context->current_bound;
        ++context->current_bound;

        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section,
                                                    5u + (spirv_size_t) expression->swizzle.items_count);
        code[0u] |= SpvOpCodeMask & SpvOpVectorShuffle;
        code[1u] = result_type_id;
        code[2u] = result_id;
        code[3u] = operand_id;
        code[4u] = operand_id;

        for (spirv_size_t index = 0u; index < (spirv_size_t) expression->swizzle.items_count; ++index)
        {
            code[5u + index] = (spirv_size_t) expression->swizzle.items[index];
        }

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_CONTAINER_FIELD_ACCESS_INPUT:
    {
        struct compiler_instance_container_field_stage_node_t *stage =
            expression->container_field_access->first_usage_stage;

        while (stage)
        {
            if (stage->user_stage == function->source->required_stage)
            {
                if (result_should_be_pointer)
                {
                    return stage->spirv_id_input;
                }

                return spirv_request_load (context, *current_block,
                                           spirv_find_or_generate_object_type (context, &expression->output, 0u),
                                           stage->spirv_id_input, KAN_TRUE);
            }

            stage = stage->next;
        }

        // Internal corruption somewhere.
        KAN_ASSERT (KAN_FALSE)
        return SPIRV_FIXED_ID_INVALID;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_CONTAINER_FIELD_ACCESS_OUTPUT:
    {
        struct compiler_instance_container_field_stage_node_t *stage =
            expression->container_field_access->first_usage_stage;

        while (stage)
        {
            if (stage->user_stage == function->source->required_stage)
            {
                if (result_should_be_pointer)
                {
                    return stage->spirv_id_output;
                }

                return spirv_request_load (context, *current_block,
                                           spirv_find_or_generate_object_type (context, &expression->output, 0u),
                                           stage->spirv_id_output, KAN_TRUE);
            }

            stage = stage->next;
        }

        // Internal corruption somewhere.
        KAN_ASSERT (KAN_FALSE)
        return SPIRV_FIXED_ID_INVALID;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_FLOATING_LITERAL:
    {
        spirv_size_t result_id = spirv_request_f1_constant (context, (float) expression->floating_literal);
        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_UNSIGNED_LITERAL:
    {
        spirv_size_t result_id =
            spirv_request_u1_constant (context, (spirv_unsigned_literal_t) expression->unsigned_literal);
        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_SIGNED_LITERAL:
    {
        spirv_size_t result_id =
            spirv_request_s1_constant (context, (spirv_signed_literal_t) expression->signed_literal);
        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_DECLARATION:
        // If variable declaration result is requested as not pointer, then something is off with resolve or AST.
        KAN_ASSERT (result_should_be_pointer)
        return expression->variable_declaration.declared_in_scope->spirv_id;

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ARRAY_INDEX:
        // Sometimes access chains can be replaced by composite extracts (for better performance),
        // but we do not support it right now.
        return spirv_emit_access_chain (context, function, current_block, expression, result_should_be_pointer);

#define BINARY_OPERATION_COMMON_PREPARE                                                                                \
    const spirv_size_t left_operand_id = spirv_emit_expression (context, function, current_block,                      \
                                                                expression->binary_operation.left_operand, KAN_FALSE); \
    const spirv_size_t right_operand_id = spirv_emit_expression (                                                      \
        context, function, current_block, expression->binary_operation.right_operand, KAN_FALSE)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ADD:
    {
        BINARY_OPERATION_COMMON_PREPARE;
        spirv_size_t result_id = SPIRV_FIXED_ID_INVALID;

        switch (expression->output.class)
        {
        case COMPILER_INSTANCE_TYPE_CLASS_VOID:
        case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
        case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
        case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
        case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
        case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
            KAN_ASSERT (KAN_FALSE)
            result_id = (spirv_size_t) SPIRV_FIXED_ID_INVALID;
            break;

        case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
            result_id = spirv_emit_vector_add (context, &(*current_block)->code_section, expression->output.vector_data,
                                               left_operand_id, right_operand_id);
            break;

        case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
            result_id = spirv_emit_matrix_add (context, &(*current_block)->code_section, expression->output.matrix_data,
                                               left_operand_id, right_operand_id);
            break;
        }

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_SUBTRACT:
    {
        BINARY_OPERATION_COMMON_PREPARE;
        spirv_size_t result_id = SPIRV_FIXED_ID_INVALID;

        switch (expression->output.class)
        {
        case COMPILER_INSTANCE_TYPE_CLASS_VOID:
        case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
        case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
        case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
        case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
        case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
            KAN_ASSERT (KAN_FALSE)
            result_id = (spirv_size_t) SPIRV_FIXED_ID_INVALID;
            break;

        case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
            result_id = spirv_emit_vector_sub (context, &(*current_block)->code_section, expression->output.vector_data,
                                               left_operand_id, right_operand_id);
            break;

        case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
            result_id = spirv_emit_matrix_sub (context, &(*current_block)->code_section, expression->output.matrix_data,
                                               left_operand_id, right_operand_id);
            break;
        }

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_MULTIPLY:
    {
        BINARY_OPERATION_COMMON_PREPARE;
        spirv_size_t result_id;

        if (expression->binary_operation.left_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
            expression->binary_operation.right_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
            expression->binary_operation.left_operand->output.vector_data ==
                expression->binary_operation.right_operand->output.vector_data)
        {
            result_id = spirv_emit_vector_mul (context, &(*current_block)->code_section, expression->output.vector_data,
                                               left_operand_id, right_operand_id);
        }
        else if (expression->binary_operation.left_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
                 expression->binary_operation.right_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
                 expression->binary_operation.right_operand->output.vector_data->items_count == 1u)
        {
            result_id = context->current_bound;
            ++context->current_bound;

            spirv_size_t *multiply = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
            multiply[0u] |= SpvOpCodeMask & SpvOpVectorTimesScalar;
            multiply[1u] = spirv_find_or_generate_vector_type (
                context, (kan_rpl_size_t) (expression->output.vector_data - STATICS.vector_types));
            multiply[2u] = result_id;
            multiply[3u] = left_operand_id;
            multiply[4u] = right_operand_id;
        }
        else if (expression->binary_operation.left_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_MATRIX &&
                 expression->binary_operation.right_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR)
        {
            result_id = context->current_bound;
            ++context->current_bound;

            if (expression->binary_operation.right_operand->output.vector_data->items_count == 1u)
            {
                spirv_size_t *multiply = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
                multiply[0u] |= SpvOpCodeMask & SpvOpMatrixTimesScalar;
                multiply[1u] = spirv_find_or_generate_matrix_type (
                    context, (kan_rpl_size_t) (expression->output.matrix_data - STATICS.matrix_types));
                multiply[2u] = result_id;
                multiply[3u] = left_operand_id;
                multiply[4u] = right_operand_id;
            }
            else
            {
                spirv_size_t *multiply = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
                multiply[0u] |= SpvOpCodeMask & SpvOpMatrixTimesVector;
                multiply[1u] = spirv_find_or_generate_vector_type (
                    context, (kan_rpl_size_t) (expression->output.vector_data - STATICS.vector_types));
                multiply[2u] = result_id;
                multiply[3u] = left_operand_id;
                multiply[4u] = right_operand_id;
            }
        }
        else if (expression->binary_operation.left_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
                 expression->binary_operation.right_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_MATRIX)
        {
            result_id = context->current_bound;
            ++context->current_bound;

            spirv_size_t *multiply = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
            multiply[0u] |= SpvOpCodeMask & SpvOpVectorTimesMatrix;
            multiply[1u] = spirv_find_or_generate_vector_type (
                context, (kan_rpl_size_t) (expression->output.vector_data - STATICS.vector_types));
            multiply[2u] = result_id;
            multiply[3u] = left_operand_id;
            multiply[4u] = right_operand_id;
        }
        else if (expression->binary_operation.left_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_MATRIX &&
                 expression->binary_operation.right_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_MATRIX)
        {
            result_id = context->current_bound;
            ++context->current_bound;

            spirv_size_t *multiply = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
            multiply[0u] |= SpvOpCodeMask & SpvOpMatrixTimesMatrix;
            multiply[1u] = spirv_find_or_generate_matrix_type (
                context, (kan_rpl_size_t) (expression->output.matrix_data - STATICS.matrix_types));
            multiply[2u] = result_id;
            multiply[3u] = left_operand_id;
            multiply[4u] = right_operand_id;
        }
        else
        {
            KAN_ASSERT (KAN_FALSE)
            result_id = (spirv_size_t) SPIRV_FIXED_ID_INVALID;
        }

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_DIVIDE:
    {
        BINARY_OPERATION_COMMON_PREPARE;
        spirv_size_t result_id;

        if (expression->binary_operation.left_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
            expression->binary_operation.right_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
            expression->binary_operation.left_operand->output.vector_data ==
                expression->binary_operation.right_operand->output.vector_data)
        {
            result_id = spirv_emit_vector_div (context, &(*current_block)->code_section, expression->output.vector_data,
                                               left_operand_id, right_operand_id);
        }
        else if (expression->binary_operation.left_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_MATRIX &&
                 expression->binary_operation.right_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_MATRIX &&
                 expression->binary_operation.left_operand->output.matrix_data ==
                     expression->binary_operation.right_operand->output.matrix_data)
        {
            result_id = spirv_emit_matrix_div (context, &(*current_block)->code_section, expression->output.matrix_data,
                                               left_operand_id, right_operand_id);
        }
        else if (expression->binary_operation.left_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
                 expression->binary_operation.right_operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
                 expression->binary_operation.right_operand->output.vector_data->items_count == 1u)
        {
            spirv_size_t composite_id = context->current_bound;
            ++context->current_bound;

            spirv_size_t *construct =
                spirv_new_instruction (context, &(*current_block)->code_section,
                                       3u + expression->binary_operation.left_operand->output.vector_data->items_count);
            construct[0u] |= SpvOpCodeMask & SpvOpCompositeConstruct;
            construct[1u] = spirv_find_or_generate_vector_type (
                context, (kan_rpl_size_t) (expression->output.vector_data - STATICS.vector_types));
            construct[2u] = composite_id;

            for (kan_loop_size_t index = 0u;
                 index < expression->binary_operation.left_operand->output.vector_data->items_count; ++index)
            {
                construct[3u + index] = right_operand_id;
            }

            result_id = spirv_emit_vector_div (context, &(*current_block)->code_section,
                                               expression->binary_operation.left_operand->output.vector_data,
                                               left_operand_id, composite_id);
        }
        else
        {
            KAN_ASSERT (KAN_FALSE)
            result_id = (spirv_size_t) SPIRV_FIXED_ID_INVALID;
        }

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_MODULUS:
    {
        BINARY_OPERATION_COMMON_PREPARE;
        spirv_size_t result_id = context->current_bound;
        ++context->current_bound;

        spirv_size_t operation = SpvOpCodeMask;
        switch (expression->output.vector_data->item)
        {
        case INBUILT_TYPE_ITEM_FLOAT:
            KAN_ASSERT (KAN_FALSE)
            break;

        case INBUILT_TYPE_ITEM_UNSIGNED:
            operation = SpvOpUMod;
            break;

        case INBUILT_TYPE_ITEM_SIGNED:
            operation = SpvOpSMod;
            break;
        }

        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
        code[0u] |= SpvOpCodeMask & operation;
        code[1u] = spirv_find_or_generate_vector_type (
            context, (kan_rpl_size_t) (expression->output.vector_data - STATICS.vector_types));
        code[2u] = result_id;
        code[3u] = left_operand_id;
        code[4u] = right_operand_id;

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ASSIGN:
    {
        const spirv_size_t left_operand_id = spirv_emit_expression (
            context, function, current_block, expression->binary_operation.left_operand, KAN_TRUE);
        const spirv_size_t right_operand_id = spirv_emit_expression (
            context, function, current_block, expression->binary_operation.right_operand, KAN_FALSE);

        spirv_emit_store (context, *current_block, left_operand_id, right_operand_id);
        return result_should_be_pointer ? left_operand_id : right_operand_id;
    }

#define SELECTIVE_LOGICAL_OPERATION(BLOCK_IF_LEFT_TRUE, BLOCK_IF_LEFT_FALSE)                                           \
    {                                                                                                                  \
        const spirv_size_t left_operand_id = spirv_emit_expression (                                                   \
            context, function, current_block, expression->binary_operation.left_operand, KAN_FALSE);                   \
                                                                                                                       \
        const spirv_size_t left_block_id = (*current_block)->spirv_id;                                                 \
        spirv_size_t right_block_id = context->current_bound;                                                          \
        ++context->current_bound;                                                                                      \
                                                                                                                       \
        spirv_size_t merge_block_id = context->current_bound;                                                          \
        ++context->current_bound;                                                                                      \
                                                                                                                       \
        spirv_size_t *selection_code = spirv_new_instruction (context, &(*current_block)->code_section, 3u);           \
        selection_code[0u] |= SpvOpCodeMask & SpvOpSelectionMerge;                                                     \
        selection_code[1u] = merge_block_id;                                                                           \
        selection_code[2u] = 0u;                                                                                       \
                                                                                                                       \
        spirv_size_t *branch_code = spirv_new_instruction (context, &(*current_block)->code_section, 4u);              \
        branch_code[0u] |= SpvOpCodeMask & SpvOpBranchConditional;                                                     \
        branch_code[1u] = left_operand_id;                                                                             \
        branch_code[2u] = BLOCK_IF_LEFT_TRUE;                                                                          \
        branch_code[3u] = BLOCK_IF_LEFT_FALSE;                                                                         \
                                                                                                                       \
        struct spirv_generation_block_t *right_block = spirv_function_new_block (context, function, right_block_id);   \
        const spirv_size_t right_operand_id = spirv_emit_expression (                                                  \
            context, function, &right_block, expression->binary_operation.right_operand, KAN_FALSE);                   \
                                                                                                                       \
        spirv_size_t *branch_merge_code = spirv_new_instruction (context, &right_block->code_section, 2u);             \
        branch_merge_code[0u] |= SpvOpCodeMask & SpvOpBranch;                                                          \
        branch_merge_code[1u] = merge_block_id;                                                                        \
                                                                                                                       \
        *current_block = spirv_function_new_block (context, function, merge_block_id);                                 \
        spirv_size_t result_id = context->current_bound;                                                               \
        ++context->current_bound;                                                                                      \
                                                                                                                       \
        spirv_size_t *phi_code = spirv_new_instruction (context, &((*current_block)->code_section), 7u);               \
        phi_code[0u] |= SpvOpCodeMask & SpvOpPhi;                                                                      \
        phi_code[1u] = (spirv_size_t) SPIRV_FIXED_ID_TYPE_BOOLEAN;                                                     \
        phi_code[2u] = result_id;                                                                                      \
        phi_code[3u] = left_operand_id;                                                                                \
        phi_code[4u] = left_block_id;                                                                                  \
        phi_code[5u] = right_operand_id;                                                                               \
        phi_code[6u] = right_block_id;                                                                                 \
                                                                                                                       \
        return result_id;                                                                                              \
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_AND:
        SELECTIVE_LOGICAL_OPERATION (right_block_id, merge_block_id)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_OR:
        SELECTIVE_LOGICAL_OPERATION (merge_block_id, right_block_id)

#define TRIVIAL_LOGICAL_OPERATION(OPERATION)                                                                           \
    {                                                                                                                  \
        BINARY_OPERATION_COMMON_PREPARE;                                                                               \
        spirv_size_t result_id = context->current_bound;                                                               \
        ++context->current_bound;                                                                                      \
                                                                                                                       \
        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 5u);                     \
        code[0u] |= SpvOpCodeMask & OPERATION;                                                                         \
        code[1u] = (spirv_size_t) SPIRV_FIXED_ID_TYPE_BOOLEAN;                                                         \
        code[2u] = result_id;                                                                                          \
        code[3u] = left_operand_id;                                                                                    \
        code[4u] = right_operand_id;                                                                                   \
                                                                                                                       \
        KAN_ASSERT (!result_should_be_pointer)                                                                         \
        return result_id;                                                                                              \
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_EQUAL:
        TRIVIAL_LOGICAL_OPERATION (SpvOpIEqual)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NOT_EQUAL:
        TRIVIAL_LOGICAL_OPERATION (SpvOpINotEqual)

#define SCALAR_LOGICAL_OPERATION(WHEN_FLOAT, WHEN_UNSIGNED, WHEN_SIGNED)                                               \
    {                                                                                                                  \
        BINARY_OPERATION_COMMON_PREPARE;                                                                               \
        spirv_size_t result_id = context->current_bound;                                                               \
        ++context->current_bound;                                                                                      \
                                                                                                                       \
        spirv_size_t operation = SpvOpCodeMask;                                                                        \
        switch (expression->binary_operation.left_operand->output.vector_data->item)                                   \
        {                                                                                                              \
        case INBUILT_TYPE_ITEM_FLOAT:                                                                                  \
            operation = WHEN_FLOAT;                                                                                    \
            break;                                                                                                     \
                                                                                                                       \
        case INBUILT_TYPE_ITEM_UNSIGNED:                                                                               \
            operation = WHEN_UNSIGNED;                                                                                 \
            break;                                                                                                     \
                                                                                                                       \
        case INBUILT_TYPE_ITEM_SIGNED:                                                                                 \
            operation = WHEN_SIGNED;                                                                                   \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 5u);                     \
        code[0u] |= SpvOpCodeMask & operation;                                                                         \
        code[1u] = (spirv_size_t) SPIRV_FIXED_ID_TYPE_BOOLEAN;                                                         \
        code[2u] = result_id;                                                                                          \
        code[3u] = left_operand_id;                                                                                    \
        code[4u] = right_operand_id;                                                                                   \
                                                                                                                       \
        KAN_ASSERT (!result_should_be_pointer)                                                                         \
        return result_id;                                                                                              \
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_LESS:
        SCALAR_LOGICAL_OPERATION (SpvOpFOrdLessThan, SpvOpULessThan, SpvOpSLessThan)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_GREATER:
        SCALAR_LOGICAL_OPERATION (SpvOpFOrdGreaterThan, SpvOpUGreaterThan, SpvOpSGreaterThan)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_LESS_OR_EQUAL:
        SCALAR_LOGICAL_OPERATION (SpvOpFOrdLessThanEqual, SpvOpULessThanEqual, SpvOpSLessThanEqual)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_GREATER_OR_EQUAL:
        SCALAR_LOGICAL_OPERATION (SpvOpFOrdGreaterThanEqual, SpvOpUGreaterThanEqual, SpvOpSGreaterThanEqual)

#define TRIVIAL_BITWISE_OPERATION(OPERATION)                                                                           \
    {                                                                                                                  \
        BINARY_OPERATION_COMMON_PREPARE;                                                                               \
        spirv_size_t result_id = context->current_bound;                                                               \
        ++context->current_bound;                                                                                      \
                                                                                                                       \
        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 5u);                     \
        code[0u] |= SpvOpCodeMask & OPERATION;                                                                         \
        code[1u] = spirv_find_or_generate_vector_type (                                                                \
            context, (kan_rpl_size_t) (expression->output.vector_data - STATICS.vector_types));                        \
        code[2u] = result_id;                                                                                          \
        code[3u] = left_operand_id;                                                                                    \
        code[4u] = right_operand_id;                                                                                   \
                                                                                                                       \
        WRAP_OPERATION_RESULT_IF_NEEDED                                                                                \
        return result_id;                                                                                              \
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_AND:
        TRIVIAL_BITWISE_OPERATION (SpvOpBitwiseAnd)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_OR:
        TRIVIAL_BITWISE_OPERATION (SpvOpBitwiseOr)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_XOR:
        TRIVIAL_BITWISE_OPERATION (SpvOpBitwiseXor)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_LEFT_SHIFT:
        TRIVIAL_BITWISE_OPERATION (SpvOpShiftLeftLogical)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_RIGHT_SHIFT:
        TRIVIAL_BITWISE_OPERATION (SpvOpShiftRightLogical)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NEGATE:
    {
        spirv_size_t operand_id =
            spirv_emit_expression (context, function, current_block, expression->unary_operation.operand, KAN_FALSE);

        spirv_size_t result_id = context->current_bound;
        ++context->current_bound;

        switch (expression->output.class)
        {
        case COMPILER_INSTANCE_TYPE_CLASS_VOID:
        case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
        case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
        case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
        case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
        case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
            // Cannot be here.
            KAN_ASSERT (KAN_FALSE)
            break;

        case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
        {
            spirv_size_t operation = SpvOpCodeMask;
            switch (expression->output.vector_data->item)
            {
            case INBUILT_TYPE_ITEM_FLOAT:
                operation = SpvOpFNegate;
                break;

            case INBUILT_TYPE_ITEM_UNSIGNED:
                KAN_ASSERT (KAN_FALSE)
                break;

            case INBUILT_TYPE_ITEM_SIGNED:
                operation = SpvOpSNegate;
                break;
            }

            spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 4u);
            code[0u] |= SpvOpCodeMask & operation;
            code[1u] = spirv_find_or_generate_vector_type (
                context, (kan_rpl_size_t) (expression->output.vector_data - STATICS.vector_types));
            code[2u] = result_id;
            code[3u] = operand_id;
            break;
        }

        case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
        {
            spirv_size_t constant_id = (spirv_size_t) SPIRV_FIXED_ID_INVALID;
            switch (expression->output.matrix_data->item)
            {
            case INBUILT_TYPE_ITEM_FLOAT:
                constant_id = spirv_request_f1_constant (context, -1.0f);
                break;

            case INBUILT_TYPE_ITEM_UNSIGNED:
                KAN_ASSERT (KAN_FALSE)
                break;

            case INBUILT_TYPE_ITEM_SIGNED:
                constant_id = spirv_request_s1_constant (context, (spirv_signed_literal_t) -1);
                break;
            }

            spirv_size_t *multiply = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
            multiply[0u] |= SpvOpCodeMask & SpvOpMatrixTimesScalar;
            multiply[1u] = spirv_find_or_generate_matrix_type (
                context, (kan_rpl_size_t) (expression->output.matrix_data - STATICS.matrix_types));
            multiply[2u] = result_id;
            multiply[3u] = operand_id;
            multiply[4u] = constant_id;
            break;
        }
        }

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NOT:
    {
        spirv_size_t operand_id =
            spirv_emit_expression (context, function, current_block, expression->unary_operation.operand, KAN_FALSE);

        spirv_size_t result_id = context->current_bound;
        ++context->current_bound;

        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 4u);
        code[0u] |= SpvOpCodeMask & SpvOpLogicalNot;
        code[1u] = (spirv_size_t) SPIRV_FIXED_ID_TYPE_BOOLEAN;
        code[2u] = result_id;
        code[3u] = operand_id;

        KAN_ASSERT (!result_should_be_pointer)
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_NOT:
    {
        spirv_size_t operand_id =
            spirv_emit_expression (context, function, current_block, expression->unary_operation.operand, KAN_FALSE);

        spirv_size_t result_id = context->current_bound;
        ++context->current_bound;

        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 4u);
        code[0u] |= SpvOpCodeMask & SpvOpNot;
        code[1u] = spirv_find_or_generate_vector_type (
            context, (kan_rpl_size_t) (expression->output.vector_data - STATICS.vector_types));
        code[2u] = result_id;
        code[3u] = operand_id;

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_FUNCTION_CALL:
    {
        if (!expression->function_call.function->body)
        {
            spirv_size_t result_id = spirv_emit_inbuilt_function_call (context, function, current_block, expression);
            if (expression->function_call.function->return_type.class != COMPILER_INSTANCE_TYPE_CLASS_VOID)
            {
                WRAP_OPERATION_RESULT_IF_NEEDED
            }

            return result_id;
        }

        kan_instance_size_t argument_count = 0u;
        spirv_size_t *arguments = spirv_gather_call_arguments (
            context, function, current_block, expression->function_call.first_argument, &argument_count, KAN_TRUE);

        spirv_size_t result_id = context->current_bound;
        ++context->current_bound;

        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 4u + argument_count);
        code[0u] |= SpvOpCodeMask & SpvOpFunctionCall;
        code[1u] = expression->function_call.function->spirv_function_type->return_type_id;
        code[2u] = result_id;
        code[3u] = expression->function_call.function->spirv_id;

        if (arguments)
        {
            memcpy (code + 4u, arguments, sizeof (spirv_size_t) * argument_count);
            for (kan_loop_size_t argument_index = 0u; argument_index < argument_count; ++argument_index)
            {
                spirv_if_temporary_variable_then_stop_using (function, arguments[argument_index]);
            }
        }

        if (expression->function_call.function->spirv_function_type->return_type_id !=
            (spirv_size_t) SPIRV_FIXED_ID_TYPE_VOID)
        {
            WRAP_OPERATION_RESULT_IF_NEEDED
        }

        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_SAMPLE:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_SAMPLE_DREF:
    {
        spirv_size_t loaded_sampler_operand =
            spirv_emit_expression (context, function, current_block, expression->image_sample.sampler, KAN_FALSE);

        spirv_size_t loaded_image_operand =
            spirv_emit_expression (context, function, current_block, expression->image_sample.image, KAN_FALSE);

        enum kan_rpl_image_type_t image_type = expression->image_sample.image->output.image_type;
        spirv_size_t sampled_image_type_id = spirv_find_or_generate_image_type (context, image_type).sampled_image;

        spirv_size_t sampled_image_result_id = context->current_bound;
        ++context->current_bound;

        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
        code[0u] |= SpvOpCodeMask & SpvOpSampledImage;
        code[1u] = sampled_image_type_id;
        code[2u] = sampled_image_result_id;
        code[3u] = loaded_image_operand;
        code[4u] = loaded_sampler_operand;

        spirv_size_t result_id = context->current_bound;
        ++context->current_bound;

        if (expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_SAMPLE)
        {
            spirv_size_t sampled_vector_type_id =
                spirv_find_or_generate_vector_type (context, INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_FLOAT, 4u));

            switch (image_type)
            {
            case KAN_RPL_IMAGE_TYPE_COLOR_2D:
            case KAN_RPL_IMAGE_TYPE_COLOR_3D:
            case KAN_RPL_IMAGE_TYPE_COLOR_CUBE:
            case KAN_RPL_IMAGE_TYPE_DEPTH_2D:
            case KAN_RPL_IMAGE_TYPE_DEPTH_3D:
            case KAN_RPL_IMAGE_TYPE_DEPTH_CUBE:
            {
                struct compiler_instance_expression_list_item_t *argument_coordinates =
                    expression->image_sample.first_argument;
                KAN_ASSERT (argument_coordinates)

                spirv_size_t coordinates_operand = spirv_emit_expression (context, function, current_block,
                                                                          argument_coordinates->expression, KAN_FALSE);

                code = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
                code[0u] |= SpvOpCodeMask & SpvOpImageSampleImplicitLod;
                code[1u] = sampled_vector_type_id;
                code[2u] = result_id;
                code[3u] = sampled_image_result_id;
                code[4u] = coordinates_operand;
                break;
            }

            case KAN_RPL_IMAGE_TYPE_COLOR_2D_ARRAY:
            case KAN_RPL_IMAGE_TYPE_DEPTH_2D_ARRAY:
            {
                struct compiler_instance_expression_list_item_t *argument_layer =
                    expression->image_sample.first_argument;
                KAN_ASSERT (argument_layer)

                struct compiler_instance_expression_list_item_t *argument_coordinates = argument_layer->next;
                KAN_ASSERT (argument_coordinates)

                spirv_size_t layer_operand =
                    spirv_emit_expression (context, function, current_block, argument_layer->expression, KAN_FALSE);

                spirv_size_t coordinates_operand = spirv_emit_expression (context, function, current_block,
                                                                          argument_coordinates->expression, KAN_FALSE);

                spirv_size_t type_f1 = spirv_find_or_generate_vector_type (
                    context, INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_FLOAT, 1u));

                spirv_size_t id_u = spirv_emit_single_composite_extract (context, &(*current_block)->code_section,
                                                                         type_f1, coordinates_operand, 0u);

                spirv_size_t id_v = spirv_emit_single_composite_extract (context, &(*current_block)->code_section,
                                                                         type_f1, coordinates_operand, 1u);

                spirv_size_t id_layer = spirv_emit_convert_signed_to_float (context, &(*current_block)->code_section,
                                                                            type_f1, layer_operand);

                spirv_size_t id_full_coordinates = context->current_bound;
                ++context->current_bound;

                spirv_size_t type_f3 = spirv_find_or_generate_vector_type (
                    context, INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_FLOAT, 3u));

                code = spirv_new_instruction (context, &(*current_block)->code_section, 6u);
                code[0u] |= SpvOpCodeMask & SpvOpCompositeConstruct;
                code[1u] = type_f3;
                code[2u] = id_full_coordinates;
                code[3u] = id_u;
                code[4u] = id_v;
                code[5u] = id_layer;

                code = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
                code[0u] |= SpvOpCodeMask & SpvOpImageSampleImplicitLod;
                code[1u] = sampled_vector_type_id;
                code[2u] = result_id;
                code[3u] = sampled_image_result_id;
                code[4u] = id_full_coordinates;
                break;
            }

            case KAN_RPL_IMAGE_TYPE_COUNT:
                KAN_ASSERT (KAN_FALSE)
                break;
            }
        }
        else
        {
            spirv_size_t sampled_vector_type_id =
                spirv_find_or_generate_vector_type (context, INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_FLOAT, 1u));

            switch (image_type)
            {
            case KAN_RPL_IMAGE_TYPE_COLOR_2D:
            case KAN_RPL_IMAGE_TYPE_COLOR_3D:
            case KAN_RPL_IMAGE_TYPE_COLOR_CUBE:
            case KAN_RPL_IMAGE_TYPE_COLOR_2D_ARRAY:
                KAN_ASSERT (KAN_FALSE)
                break;

            case KAN_RPL_IMAGE_TYPE_DEPTH_2D:
            case KAN_RPL_IMAGE_TYPE_DEPTH_3D:
            case KAN_RPL_IMAGE_TYPE_DEPTH_CUBE:
            {
                struct compiler_instance_expression_list_item_t *argument_coordinates =
                    expression->image_sample.first_argument;
                KAN_ASSERT (argument_coordinates)

                struct compiler_instance_expression_list_item_t *argument_reference = argument_coordinates->next;
                KAN_ASSERT (argument_reference)

                spirv_size_t coordinates_operand = spirv_emit_expression (context, function, current_block,
                                                                          argument_coordinates->expression, KAN_FALSE);

                spirv_size_t reference_operand =
                    spirv_emit_expression (context, function, current_block, argument_reference->expression, KAN_FALSE);

                code = spirv_new_instruction (context, &(*current_block)->code_section, 6u);
                code[0u] |= SpvOpCodeMask & SpvOpImageSampleDrefImplicitLod;
                code[1u] = sampled_vector_type_id;
                code[2u] = result_id;
                code[3u] = sampled_image_result_id;
                code[4u] = coordinates_operand;
                code[5u] = reference_operand;
                break;
            }

            case KAN_RPL_IMAGE_TYPE_DEPTH_2D_ARRAY:
            {
                struct compiler_instance_expression_list_item_t *argument_layer =
                    expression->image_sample.first_argument;
                KAN_ASSERT (argument_layer)

                struct compiler_instance_expression_list_item_t *argument_coordinates = argument_layer->next;
                KAN_ASSERT (argument_coordinates)

                struct compiler_instance_expression_list_item_t *argument_reference = argument_coordinates->next;
                KAN_ASSERT (argument_reference)

                spirv_size_t layer_operand =
                    spirv_emit_expression (context, function, current_block, argument_layer->expression, KAN_FALSE);

                spirv_size_t coordinates_operand = spirv_emit_expression (context, function, current_block,
                                                                          argument_coordinates->expression, KAN_FALSE);

                spirv_size_t reference_operand =
                    spirv_emit_expression (context, function, current_block, argument_reference->expression, KAN_FALSE);

                spirv_size_t type_f1 = spirv_find_or_generate_vector_type (
                    context, INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_FLOAT, 1u));

                spirv_size_t id_u = spirv_emit_single_composite_extract (context, &(*current_block)->code_section,
                                                                         type_f1, coordinates_operand, 0u);

                spirv_size_t id_v = spirv_emit_single_composite_extract (context, &(*current_block)->code_section,
                                                                         type_f1, coordinates_operand, 1u);

                spirv_size_t id_layer = spirv_emit_convert_signed_to_float (context, &(*current_block)->code_section,
                                                                            type_f1, layer_operand);

                spirv_size_t id_full_coordinates = context->current_bound;
                ++context->current_bound;

                spirv_size_t type_f3 = spirv_find_or_generate_vector_type (
                    context, INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_FLOAT, 3u));

                code = spirv_new_instruction (context, &(*current_block)->code_section, 6u);
                code[0u] |= SpvOpCodeMask & SpvOpCompositeConstruct;
                code[1u] = type_f3;
                code[2u] = id_full_coordinates;
                code[3u] = id_u;
                code[4u] = id_v;
                code[5u] = id_layer;

                code = spirv_new_instruction (context, &(*current_block)->code_section, 6u);
                code[0u] |= SpvOpCodeMask & SpvOpImageSampleDrefImplicitLod;
                code[1u] = sampled_vector_type_id;
                code[2u] = result_id;
                code[3u] = sampled_image_result_id;
                code[4u] = id_full_coordinates;
                code[4u] = reference_operand;
                break;
            }

            case KAN_RPL_IMAGE_TYPE_COUNT:
                KAN_ASSERT (KAN_FALSE)
                break;
            }
        }

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_VECTOR_CONSTRUCTOR:
    {
        switch (expression->vector_constructor.variant)
        {
        case COMPILER_INSTANCE_VECTOR_CONSTRUCTOR_SKIP:
        {
            // No actual construction, just forward the result of the expression.
            return spirv_emit_expression (context, function, current_block,
                                          expression->vector_constructor.first_argument->expression,
                                          result_should_be_pointer);
        }

        case COMPILER_INSTANCE_VECTOR_CONSTRUCTOR_COMBINE:
        {
            kan_instance_size_t argument_count = 0u;
            spirv_size_t *arguments =
                spirv_gather_call_arguments (context, function, current_block,
                                             expression->vector_constructor.first_argument, &argument_count, KAN_FALSE);

            spirv_size_t result_id = context->current_bound;
            ++context->current_bound;
            spirv_size_t result_type_id = spirv_find_or_generate_vector_type (
                context, (kan_rpl_size_t) (expression->vector_constructor.type - STATICS.vector_types));

            // Despite the fact that arguments can be vectors, SPIRV specification explicitly allows to
            // use vector arguments as scalar sequences inside composite constructor for vector type.
            spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 3u + argument_count);
            code[0u] |= SpvOpCodeMask & SpvOpCompositeConstruct;
            code[1u] = result_type_id;
            code[2u] = result_id;

            if (arguments)
            {
                memcpy (code + 3u, arguments, sizeof (spirv_size_t) * argument_count);
            }

            WRAP_OPERATION_RESULT_IF_NEEDED
            return result_id;
        }

        case COMPILER_INSTANCE_VECTOR_CONSTRUCTOR_CONVERT:
        {
            spirv_size_t operand_id = spirv_emit_expression (
                context, function, current_block, expression->vector_constructor.first_argument->expression, KAN_FALSE);

            spirv_size_t result_type_id = spirv_find_or_generate_vector_type (
                context, (kan_rpl_size_t) (expression->vector_constructor.type - STATICS.vector_types));

            spirv_size_t result_id = spirv_convert_vector (
                context, &(*current_block)->code_section, expression->output.vector_data->item,
                expression->vector_constructor.first_argument->expression->output.vector_data->item, operand_id,
                result_type_id);

            WRAP_OPERATION_RESULT_IF_NEEDED
            return result_id;
        }

        case COMPILER_INSTANCE_VECTOR_CONSTRUCTOR_FILL:
        {
            spirv_size_t operand_id = spirv_emit_expression (
                context, function, current_block, expression->vector_constructor.first_argument->expression, KAN_FALSE);

            spirv_size_t result_id = context->current_bound;
            ++context->current_bound;
            spirv_size_t result_type_id = spirv_find_or_generate_vector_type (
                context, (kan_rpl_size_t) (expression->vector_constructor.type - STATICS.vector_types));

            spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section,
                                                        3u + expression->vector_constructor.type->items_count);
            code[0u] |= SpvOpCodeMask & SpvOpCompositeConstruct;
            code[1u] = result_type_id;
            code[2u] = result_id;

            for (spirv_size_t item_index = 0u; item_index < expression->vector_constructor.type->items_count;
                 ++item_index)
            {
                code[3u + item_index] = operand_id;
            }

            WRAP_OPERATION_RESULT_IF_NEEDED
            return result_id;
        }
        }

        KAN_ASSERT (KAN_FALSE)
        return SPIRV_FIXED_ID_INVALID;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_MATRIX_CONSTRUCTOR:
    {
        switch (expression->matrix_constructor.variant)
        {
        case COMPILER_INSTANCE_MATRIX_CONSTRUCTOR_SKIP:
            // No actual construction, just forward the result of the expression.
            return spirv_emit_expression (context, function, current_block,
                                          expression->matrix_constructor.first_argument->expression,
                                          result_should_be_pointer);

        case COMPILER_INSTANCE_MATRIX_CONSTRUCTOR_COMBINE:
        {
            kan_instance_size_t argument_count = 0u;
            spirv_size_t *arguments =
                spirv_gather_call_arguments (context, function, current_block,
                                             expression->matrix_constructor.first_argument, &argument_count, KAN_FALSE);

            spirv_size_t result_id = context->current_bound;
            ++context->current_bound;
            spirv_size_t result_type_id = spirv_find_or_generate_matrix_type (
                context, (kan_rpl_size_t) (expression->matrix_constructor.type - STATICS.matrix_types));

            spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 3u + argument_count);
            code[0u] |= SpvOpCodeMask & SpvOpCompositeConstruct;
            code[1u] = result_type_id;
            code[2u] = result_id;

            if (arguments)
            {
                memcpy (code + 3u, arguments, sizeof (spirv_size_t) * argument_count);
            }

            WRAP_OPERATION_RESULT_IF_NEEDED
            return result_id;
        }

        case COMPILER_INSTANCE_MATRIX_CONSTRUCTOR_CONVERT:
        case COMPILER_INSTANCE_MATRIX_CONSTRUCTOR_CROP:
        {
            spirv_size_t operand_id = spirv_emit_expression (
                context, function, current_block, expression->matrix_constructor.first_argument->expression, KAN_FALSE);

            struct inbuilt_matrix_type_t *operand_matrix_type =
                expression->matrix_constructor.first_argument->expression->output.matrix_data;

            spirv_size_t operand_column_type_id = spirv_find_or_generate_vector_type (
                context, INBUILT_VECTOR_TYPE_INDEX (operand_matrix_type->item, operand_matrix_type->rows));

            spirv_size_t result_type_id = spirv_find_or_generate_matrix_type (
                context, (kan_rpl_size_t) (expression->matrix_constructor.type - STATICS.matrix_types));

            struct inbuilt_matrix_type_t *result_matrix_type = expression->matrix_constructor.type;
            spirv_size_t result_column_type_id = spirv_find_or_generate_vector_type (
                context, INBUILT_VECTOR_TYPE_INDEX (result_matrix_type->item, result_matrix_type->rows));

            spirv_size_t column_ids[INBUILT_MATRIX_MAX_COLUMNS];
            if (expression->matrix_constructor.variant == COMPILER_INSTANCE_MATRIX_CONSTRUCTOR_CONVERT)
            {
                for (spirv_size_t column_index = 0u; column_index < result_matrix_type->columns; ++column_index)
                {
                    spirv_size_t extracted = spirv_emit_single_composite_extract (
                        context, &(*current_block)->code_section, operand_column_type_id, operand_id, column_index);

                    column_ids[column_index] =
                        spirv_convert_vector (context, &(*current_block)->code_section, result_matrix_type->item,
                                              operand_matrix_type->item, extracted, result_column_type_id);
                }
            }
            else
            {
                KAN_ASSERT (expression->matrix_constructor.variant == COMPILER_INSTANCE_MATRIX_CONSTRUCTOR_CROP)
                for (spirv_size_t column_index = 0u; column_index < result_matrix_type->columns; ++column_index)
                {
                    spirv_size_t extracted = spirv_emit_single_composite_extract (
                        context, &(*current_block)->code_section, operand_column_type_id, operand_id, column_index);

                    column_ids[column_index] = context->current_bound;
                    ++context->current_bound;

                    spirv_size_t *code =
                        spirv_new_instruction (context, &(*current_block)->code_section, 5u + result_matrix_type->rows);
                    code[0u] |= SpvOpCodeMask & SpvOpVectorShuffle;
                    code[1u] = result_column_type_id;
                    code[2u] = column_ids[column_index];
                    code[3u] = extracted;
                    code[4u] = extracted;

                    for (spirv_size_t row = 0u; row < result_matrix_type->rows; ++row)
                    {
                        code[5u + row] = row;
                    }
                }
            }

            spirv_size_t result_id = context->current_bound;
            ++context->current_bound;

            spirv_size_t *code =
                spirv_new_instruction (context, &(*current_block)->code_section, 3u + result_matrix_type->columns);
            code[0u] |= SpvOpCodeMask & SpvOpCompositeConstruct;
            code[1u] = result_type_id;
            code[2u] = result_id;
            memcpy (code + 3u, column_ids, sizeof (spirv_size_t) * result_matrix_type->columns);

            WRAP_OPERATION_RESULT_IF_NEEDED
            return result_id;
        }
        }

        KAN_ASSERT (KAN_FALSE)
        return SPIRV_FIXED_ID_INVALID;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCT_CONSTRUCTOR:
    {
        kan_instance_size_t argument_count = 0u;
        spirv_size_t *arguments =
            spirv_gather_call_arguments (context, function, current_block,
                                         expression->struct_constructor.first_argument, &argument_count, KAN_FALSE);

        spirv_size_t result_id = context->current_bound;
        ++context->current_bound;
        spirv_size_t result_type_id = expression->struct_constructor.type->spirv_id_value;

        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 3u + argument_count);
        code[0u] |= SpvOpCodeMask & SpvOpCompositeConstruct;
        code[1u] = result_type_id;
        code[2u] = result_id;

        if (arguments)
        {
            memcpy (code + 3u, arguments, sizeof (spirv_size_t) * argument_count);
        }

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_SCOPE:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_IF:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_FOR:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_WHILE:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_BREAK:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_CONTINUE:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_RETURN:
        // Should be processes along statements.
        KAN_ASSERT (KAN_FALSE)
        return (spirv_size_t) SPIRV_FIXED_ID_INVALID;
    }

#undef WRAP_OPERATION_RESULT_IF_NEEDED
#undef BINARY_OPERATION_COMMON_PREPARE
#undef TRIVIAL_LOGICAL_OPERATION
#undef SCALAR_LOGICAL_OPERATION
#undef TRIVIAL_BITWISE_OPERATION

    KAN_ASSERT (KAN_FALSE)
    return (spirv_size_t) SPIRV_FIXED_ID_INVALID;
}

static struct spirv_generation_block_t *spirv_emit_scope (struct spirv_generation_context_t *context,
                                                          struct spirv_generation_function_node_t *function,
                                                          struct spirv_generation_block_t *current_block,
                                                          spirv_size_t next_block_id,
                                                          struct compiler_instance_expression_node_t *scope_expression)
{
    const kan_bool_t inlined_scope = current_block->spirv_id == next_block_id;
    KAN_ASSERT (scope_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_SCOPE)
    struct compiler_instance_scope_variable_item_t *variable = scope_expression->scope.first_variable;

    while (variable)
    {
        variable->spirv_id = context->current_bound;
        ++context->current_bound;

        spirv_size_t *variable_code = spirv_new_instruction (context, &function->first_block->header_section, 4u);
        variable_code[0u] |= SpvOpCodeMask & SpvOpVariable;
        variable_code[1u] = spirv_get_or_create_pointer_type (
            context, spirv_find_or_generate_object_type (context, &variable->variable->type, 0u),
            SpvStorageClassFunction);
        variable_code[2u] = variable->spirv_id;
        variable_code[3u] = SpvStorageClassFunction;

        spirv_generate_op_name (context, variable->spirv_id, variable->variable->name);
        variable = variable->next;
    }

    struct compiler_instance_expression_list_item_t *statement = scope_expression->scope.first_expression;
    while (statement)
    {
        switch (statement->expression->type)
        {
        case COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_SAMPLER_REFERENCE:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_REFERENCE:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_REFERENCE:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_SWIZZLE:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_CONTAINER_FIELD_ACCESS_INPUT:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_CONTAINER_FIELD_ACCESS_OUTPUT:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_FLOATING_LITERAL:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_UNSIGNED_LITERAL:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_SIGNED_LITERAL:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_DECLARATION:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ARRAY_INDEX:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ADD:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_SUBTRACT:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_MULTIPLY:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_DIVIDE:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_MODULUS:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ASSIGN:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_AND:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_OR:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_EQUAL:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NOT_EQUAL:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_LESS:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_GREATER:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_LESS_OR_EQUAL:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_GREATER_OR_EQUAL:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_AND:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_OR:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_XOR:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_LEFT_SHIFT:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_RIGHT_SHIFT:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NEGATE:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NOT:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_NOT:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_FUNCTION_CALL:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_SAMPLE:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_SAMPLE_DREF:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_VECTOR_CONSTRUCTOR:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_MATRIX_CONSTRUCTOR:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCT_CONSTRUCTOR:
        {
            spirv_size_t result_id =
                spirv_emit_expression (context, function, &current_block, statement->expression, KAN_TRUE);
            spirv_if_temporary_variable_then_stop_using (function, result_id);
            break;
        }

        case COMPILER_INSTANCE_EXPRESSION_TYPE_SCOPE:
        {
            current_block = spirv_emit_scope (context, function, current_block,
                                              // Inlining scope, therefore its output is current block too.
                                              current_block->spirv_id, statement->expression);
            break;
        }

        case COMPILER_INSTANCE_EXPRESSION_TYPE_IF:
        {
            spirv_size_t condition_id = spirv_emit_expression (context, function, &current_block,
                                                               statement->expression->if_.condition, KAN_FALSE);

            struct spirv_generation_block_t *true_block =
                spirv_function_new_block (context, function, context->current_bound++);

            // Merge and false blocks are predeclared and added later
            // in order to maintain block position requirement by SPIRV.
            spirv_size_t false_block_id = (spirv_size_t) SPIRV_FIXED_ID_INVALID;

            if (statement->expression->if_.when_false)
            {
                false_block_id = context->current_bound;
                ++context->current_bound;
            }

            spirv_size_t merge_block_id = context->current_bound;
            ++context->current_bound;

            spirv_size_t *selection_code = spirv_new_instruction (context, &current_block->code_section, 3u);
            selection_code[0u] |= SpvOpCodeMask & SpvOpSelectionMerge;
            selection_code[1u] = merge_block_id;
            selection_code[2u] = 0u;

            spirv_size_t *branch_code = spirv_new_instruction (context, &current_block->code_section, 4u);
            branch_code[0u] |= SpvOpCodeMask & SpvOpBranchConditional;
            branch_code[1u] = condition_id;
            branch_code[2u] = true_block->spirv_id;
            branch_code[3u] = false_block_id != (spirv_size_t) SPIRV_FIXED_ID_INVALID ? false_block_id : merge_block_id;
            spirv_emit_scope (context, function, true_block, merge_block_id, statement->expression->if_.when_true);

            if (false_block_id != (spirv_size_t) SPIRV_FIXED_ID_INVALID)
            {
                spirv_emit_scope (context, function, spirv_function_new_block (context, function, false_block_id),
                                  merge_block_id, statement->expression->if_.when_false);
            }

            current_block = spirv_function_new_block (context, function, merge_block_id);

            // Special case -- unreachable merge block after if's.
            if (!statement->next && (scope_expression->scope.leads_to_return || scope_expression->scope.leads_to_jump))
            {
                spirv_size_t *unreachable_code = spirv_new_instruction (context, &current_block->code_section, 1u);
                unreachable_code[0u] |= SpvOpCodeMask & SpvOpUnreachable;
            }

            break;
        }

        case COMPILER_INSTANCE_EXPRESSION_TYPE_FOR:
        {
            struct spirv_generation_block_t *loop_header_block =
                spirv_function_new_block (context, function, context->current_bound++);

            struct spirv_generation_block_t *condition_begin_block =
                spirv_function_new_block (context, function, context->current_bound++);
            struct spirv_generation_block_t *condition_done_block = condition_begin_block;

            // These blocks are predeclared and added later in order to maintain block position requirement by SPIRV.
            spirv_size_t step_block_id = context->current_bound;
            ++context->current_bound;

            spirv_size_t loop_block_id = context->current_bound;
            ++context->current_bound;

            spirv_size_t merge_block_id = context->current_bound;
            ++context->current_bound;

            statement->expression->for_.spirv_label_break = merge_block_id;
            statement->expression->for_.spirv_label_continue = step_block_id;
            spirv_emit_expression (context, function, &current_block, statement->expression->for_.init, KAN_FALSE);

            spirv_size_t *enter_loop_header_code = spirv_new_instruction (context, &current_block->code_section, 2u);
            enter_loop_header_code[0u] |= SpvOpCodeMask & SpvOpBranch;
            enter_loop_header_code[1u] = loop_header_block->spirv_id;

            spirv_size_t *loop_code = spirv_new_instruction (context, &loop_header_block->code_section, 4u);
            loop_code[0u] |= SpvOpCodeMask & SpvOpLoopMerge;
            loop_code[1u] = merge_block_id;
            loop_code[2u] = step_block_id;
            loop_code[3u] = 0u;

            spirv_size_t *start_loop_code = spirv_new_instruction (context, &loop_header_block->code_section, 2u);
            start_loop_code[0u] |= SpvOpCodeMask & SpvOpBranch;
            start_loop_code[1u] = condition_begin_block->spirv_id;

            spirv_size_t condition_id = spirv_emit_expression (context, function, &condition_done_block,
                                                               statement->expression->for_.condition, KAN_FALSE);

            spirv_size_t *branch_code = spirv_new_instruction (context, &condition_done_block->code_section, 4u);
            branch_code[0u] |= SpvOpCodeMask & SpvOpBranchConditional;
            branch_code[1u] = condition_id;
            branch_code[2u] = loop_block_id;
            branch_code[3u] = merge_block_id;

            struct spirv_generation_block_t *loop_block = spirv_function_new_block (context, function, loop_block_id);
            spirv_emit_scope (context, function, loop_block, step_block_id, statement->expression->for_.body);

            struct spirv_generation_block_t *step_block = spirv_function_new_block (context, function, step_block_id);
            spirv_emit_expression (context, function, &step_block, statement->expression->for_.step, KAN_FALSE);
            spirv_size_t *continue_after_step_code = spirv_new_instruction (context, &step_block->code_section, 2u);
            continue_after_step_code[0u] |= SpvOpCodeMask & SpvOpBranch;
            continue_after_step_code[1u] = loop_header_block->spirv_id;

            current_block = spirv_function_new_block (context, function, merge_block_id);
            break;
        }

        case COMPILER_INSTANCE_EXPRESSION_TYPE_WHILE:
        {
            struct spirv_generation_block_t *loop_header_block =
                spirv_function_new_block (context, function, context->current_bound++);

            struct spirv_generation_block_t *condition_begin_block =
                spirv_function_new_block (context, function, context->current_bound++);
            struct spirv_generation_block_t *condition_done_block = condition_begin_block;

            // These blocks are predeclared and added later in order to maintain block position requirement by SPIRV.
            spirv_size_t merge_block_id = context->current_bound;
            ++context->current_bound;

            spirv_size_t loop_block_id = context->current_bound;
            ++context->current_bound;

            spirv_size_t continue_block_id = context->current_bound;
            ++context->current_bound;

            statement->expression->while_.spirv_label_break = merge_block_id;
            statement->expression->while_.spirv_label_continue = continue_block_id;

            spirv_size_t *enter_loop_header_code = spirv_new_instruction (context, &current_block->code_section, 2u);
            enter_loop_header_code[0u] |= SpvOpCodeMask & SpvOpBranch;
            enter_loop_header_code[1u] = loop_header_block->spirv_id;

            spirv_size_t *loop_code = spirv_new_instruction (context, &loop_header_block->code_section, 4u);
            loop_code[0u] |= SpvOpCodeMask & SpvOpLoopMerge;
            loop_code[1u] = merge_block_id;
            loop_code[2u] = continue_block_id;
            loop_code[3u] = 0u;

            spirv_size_t *start_loop_code = spirv_new_instruction (context, &loop_header_block->code_section, 2u);
            start_loop_code[0u] |= SpvOpCodeMask & SpvOpBranch;
            start_loop_code[1u] = condition_begin_block->spirv_id;

            spirv_size_t condition_id = spirv_emit_expression (context, function, &condition_done_block,
                                                               statement->expression->while_.condition, KAN_FALSE);

            spirv_size_t *branch_code = spirv_new_instruction (context, &condition_done_block->code_section, 4u);
            branch_code[0u] |= SpvOpCodeMask & SpvOpBranchConditional;
            branch_code[1u] = condition_id;
            branch_code[2u] = loop_block_id;
            branch_code[3u] = merge_block_id;

            struct spirv_generation_block_t *loop_block = spirv_function_new_block (context, function, loop_block_id);
            spirv_emit_scope (context, function, loop_block, continue_block_id, statement->expression->while_.body);

            struct spirv_generation_block_t *continue_block =
                spirv_function_new_block (context, function, continue_block_id);
            spirv_size_t *continue_after_continue_code =
                spirv_new_instruction (context, &continue_block->code_section, 2u);
            continue_after_continue_code[0u] |= SpvOpCodeMask & SpvOpBranch;
            continue_after_continue_code[1u] = loop_header_block->spirv_id;

            current_block = spirv_function_new_block (context, function, merge_block_id);
            break;
        }

        case COMPILER_INSTANCE_EXPRESSION_TYPE_BREAK:
        {
            spirv_size_t *branch_code = spirv_new_instruction (context, &current_block->code_section, 2u);
            branch_code[0u] |= SpvOpCodeMask & SpvOpBranch;

            if (statement->expression->break_loop->type == COMPILER_INSTANCE_EXPRESSION_TYPE_FOR)
            {
                branch_code[1u] = statement->expression->break_loop->for_.spirv_label_break;
            }
            else if (statement->expression->break_loop->type == COMPILER_INSTANCE_EXPRESSION_TYPE_WHILE)
            {
                branch_code[1u] = statement->expression->break_loop->while_.spirv_label_break;
            }
            else
            {
                // If this assert is hit, then resolve has skipped invalid break.
                KAN_ASSERT (KAN_FALSE)
                branch_code[1u] = (spirv_size_t) SPIRV_FIXED_ID_INVALID;
            }

            break;
        }

        case COMPILER_INSTANCE_EXPRESSION_TYPE_CONTINUE:
        {
            spirv_size_t *branch_code = spirv_new_instruction (context, &current_block->code_section, 2u);
            branch_code[0u] |= SpvOpCodeMask & SpvOpBranch;

            if (statement->expression->continue_loop->type == COMPILER_INSTANCE_EXPRESSION_TYPE_FOR)
            {
                branch_code[1u] = statement->expression->continue_loop->for_.spirv_label_continue;
            }
            else if (statement->expression->continue_loop->type == COMPILER_INSTANCE_EXPRESSION_TYPE_WHILE)
            {
                branch_code[1u] = statement->expression->continue_loop->while_.spirv_label_continue;
            }
            else
            {
                // If this assert is hit, then resolve has skipped invalid continue.
                KAN_ASSERT (KAN_FALSE)
                branch_code[1u] = (spirv_size_t) SPIRV_FIXED_ID_INVALID;
            }

            continue;
        }

        case COMPILER_INSTANCE_EXPRESSION_TYPE_RETURN:
        {
            if (statement->expression->return_expression)
            {
                spirv_size_t result_id = spirv_emit_expression (context, function, &current_block,
                                                                statement->expression->return_expression, KAN_FALSE);
                spirv_size_t *return_code = spirv_new_instruction (context, &current_block->code_section, 2u);
                return_code[0u] |= SpvOpCodeMask & SpvOpReturnValue;
                return_code[1u] = result_id;
            }
            else
            {
                spirv_size_t *return_code = spirv_new_instruction (context, &current_block->code_section, 1u);
                return_code[0u] |= SpvOpCodeMask & SpvOpReturn;
            }

            break;
        }
        }

        statement = statement->next;
    }

    if (!inlined_scope && next_block_id != (spirv_size_t) SPIRV_FIXED_ID_INVALID &&
        !scope_expression->scope.leads_to_return && !scope_expression->scope.leads_to_jump)
    {
        spirv_size_t *branch_code = spirv_new_instruction (context, &current_block->code_section, 2u);
        branch_code[0u] |= SpvOpCodeMask & SpvOpBranch;
        branch_code[1u] = next_block_id;
    }

    return current_block;
}

static inline void spirv_emit_function (struct spirv_generation_context_t *context,
                                        struct compiler_instance_function_node_t *function)
{
    struct spirv_generation_function_node_t *generated_function = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &context->temporary_allocator, struct spirv_generation_function_node_t);

    generated_function->source = function;
    generated_function->header_section.first = NULL;
    generated_function->header_section.last = NULL;
    generated_function->first_block = NULL;
    generated_function->last_block = NULL;
    generated_function->end_section.first = NULL;
    generated_function->end_section.last = NULL;
    generated_function->first_free_temporary_variable = NULL;
    generated_function->first_used_temporary_variable = NULL;

    generated_function->next = NULL;
    if (context->last_function_node)
    {
        context->last_function_node->next = generated_function;
    }
    else
    {
        context->first_function_node = generated_function;
    }

    context->last_function_node = generated_function;
    spirv_generate_op_name (context, function->spirv_id, function->name);

    spirv_size_t *definition_code = spirv_new_instruction (context, &generated_function->header_section, 5u);
    definition_code[0u] |= SpvOpCodeMask & SpvOpFunction;
    definition_code[1u] = function->spirv_function_type->return_type_id;
    definition_code[2u] = function->spirv_id;
    definition_code[4u] = function->spirv_function_type->generated_id;

    kan_bool_t writes_globals = KAN_FALSE;
    kan_bool_t reads_globals = function->first_container_access || function->first_buffer_access ||
                               function->first_sampler_access || function->first_image_access;

    struct compiler_instance_container_access_node_t *container_access = function->first_container_access;
    while (container_access)
    {
        switch (container_access->container->type)
        {
        case KAN_RPL_CONTAINER_TYPE_VERTEX_ATTRIBUTE:
        case KAN_RPL_CONTAINER_TYPE_INSTANCED_ATTRIBUTE:
            break;

        case KAN_RPL_CONTAINER_TYPE_STATE:
            switch (function->required_stage)
            {
            case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX:
                writes_globals = KAN_TRUE;
                break;

            case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT:
                break;
            }

            break;

        case KAN_RPL_CONTAINER_TYPE_COLOR_OUTPUT:
            switch (function->required_stage)
            {
            case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX:
                KAN_ASSERT (KAN_FALSE)
                break;

            case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT:
                writes_globals = KAN_TRUE;
                break;
            }

            break;
        }

        container_access = container_access->next;
    }

    if (!writes_globals && !reads_globals)
    {
        definition_code[3u] = SpvFunctionControlConstMask;
    }
    else if (!reads_globals)
    {
        definition_code[3u] = SpvFunctionControlPureMask;
    }
    else
    {
        definition_code[3u] = SpvFunctionControlMaskNone;
    }

    struct compiler_instance_scope_variable_item_t *argument_variable = function->first_argument_variable;
    while (argument_variable)
    {
        argument_variable->spirv_id = context->current_bound;
        ++context->current_bound;

        spirv_size_t *argument_code = spirv_new_instruction (context, &generated_function->header_section, 3u);
        argument_code[0u] |= SpvOpCodeMask & SpvOpFunctionParameter;
        argument_code[1u] = spirv_get_or_create_pointer_type (
            context, spirv_find_or_generate_object_type (context, &argument_variable->variable->type, 0u),
            spirv_is_uniform_resource_type (&argument_variable->variable->type) ? SpvStorageClassUniformConstant :
                                                                                  SpvStorageClassFunction);
        argument_code[2u] = argument_variable->spirv_id;

        spirv_generate_op_name (context, argument_variable->spirv_id, argument_variable->variable->name);
        argument_variable = argument_variable->next;
    }

    spirv_size_t *end_code = spirv_new_instruction (context, &generated_function->end_section, 1u);
    end_code[0u] |= SpvOpCodeMask & SpvOpFunctionEnd;

    struct spirv_generation_block_t *function_block =
        spirv_function_new_block (context, generated_function, context->current_bound++);

    function_block = spirv_emit_scope (context, generated_function, function_block,
                                       (spirv_size_t) SPIRV_FIXED_ID_INVALID, function->body);

    if (!function->body->scope.leads_to_return)
    {
        spirv_size_t *return_code = spirv_new_instruction (context, &function_block->code_section, 1u);
        return_code[0u] |= SpvOpCodeMask & SpvOpReturn;
    }
}

kan_bool_t kan_rpl_compiler_instance_emit_spirv (kan_rpl_compiler_instance_t compiler_instance,
                                                 struct kan_dynamic_array_t *output,
                                                 kan_allocation_group_t output_allocation_group)
{
    kan_dynamic_array_init (output, 0u, sizeof (spirv_size_t), _Alignof (spirv_size_t), output_allocation_group);
    struct rpl_compiler_instance_t *instance = KAN_HANDLE_GET (compiler_instance);
    struct spirv_generation_context_t context;
    spirv_init_generation_context (&context, instance);

    struct compiler_instance_struct_node_t *struct_node = instance->first_struct;
    while (struct_node)
    {
        struct_node->spirv_id_value = context.current_bound;
        ++context.current_bound;
        spirv_emit_struct_from_declaration_list (&context, struct_node->first_field, struct_node->name,
                                                 struct_node->spirv_id_value);

        struct_node->spirv_id_function_pointer = context.current_bound;
        ++context.current_bound;

        spirv_register_and_generate_known_pointer_type (&context, &context.higher_type_section,
                                                        struct_node->spirv_id_function_pointer,
                                                        struct_node->spirv_id_value, SpvStorageClassFunction);
        struct_node = struct_node->next;
    }

    struct compiler_instance_container_node_t *container = instance->first_container;
    while (container)
    {
        if (!container->used)
        {
            container = container->next;
            continue;
        }

        struct compiler_instance_container_field_node_t *field = container->first_field;
        while (field)
        {
            spirv_emit_container_field (&context, container->type, field);
            field = field->next;
        }

        container = container->next;
    }

    struct compiler_instance_buffer_node_t *buffer = instance->first_buffer;
    while (buffer)
    {
        if (!buffer->used)
        {
            buffer = buffer->next;
            continue;
        }

        spirv_size_t buffer_struct_id = context.current_bound;
        ++context.current_bound;
        spirv_emit_struct_from_declaration_list (&context, buffer->first_field, buffer->name, buffer_struct_id);

        switch (buffer->type)
        {
        case KAN_RPL_BUFFER_TYPE_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
        case KAN_RPL_BUFFER_TYPE_PUSH_CONSTANT:
        {
            spirv_size_t *block_decorate_code = spirv_new_instruction (&context, &context.decoration_section, 3u);
            block_decorate_code[0u] |= SpvOpCodeMask & SpvOpDecorate;
            block_decorate_code[1u] = buffer_struct_id;
            block_decorate_code[2u] = SpvDecorationBlock;
            break;
        }
        }

        SpvStorageClass storage_type = spirv_get_structured_buffer_storage_class (buffer);
        spirv_size_t buffer_struct_pointer_id = context.current_bound;
        ++context.current_bound;

        spirv_size_t *pointer_code = spirv_new_instruction (&context, &context.higher_type_section, 4u);
        pointer_code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
        pointer_code[1u] = buffer_struct_pointer_id;
        pointer_code[2u] = storage_type;
        pointer_code[3u] = buffer_struct_id;

        buffer->structured_variable_spirv_id = context.current_bound;
        ++context.current_bound;

        spirv_size_t *variable_code = spirv_new_instruction (&context, &context.global_variable_section, 4u);
        variable_code[0u] |= SpvOpCodeMask & SpvOpVariable;
        variable_code[1u] = buffer_struct_pointer_id;
        variable_code[2u] = buffer->structured_variable_spirv_id;
        variable_code[3u] = storage_type;

        switch (buffer->type)
        {
        case KAN_RPL_BUFFER_TYPE_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
            spirv_emit_descriptor_set (&context, buffer->structured_variable_spirv_id, (spirv_size_t) buffer->set);
            spirv_emit_binding (&context, buffer->structured_variable_spirv_id, buffer->binding);
            break;

        case KAN_RPL_BUFFER_TYPE_PUSH_CONSTANT:
            // No descriptor sets or bindings for push constants.
            break;
        }

        spirv_generate_op_name (&context, buffer->structured_variable_spirv_id, buffer->name);
        buffer = buffer->next;
    }

    struct compiler_instance_sampler_node_t *sampler = instance->first_sampler;
    while (sampler)
    {
        if (!sampler->used)
        {
            sampler = sampler->next;
            continue;
        }

        sampler->variable_spirv_id = context.current_bound;
        ++context.current_bound;

        spirv_size_t *sampler_code = spirv_new_instruction (&context, &context.global_variable_section, 4u);
        sampler_code[0u] |= SpvOpCodeMask & SpvOpVariable;
        sampler_code[1u] = spirv_get_or_create_pointer_type (&context, spirv_find_or_generate_sampler_type (&context),
                                                             SpvStorageClassUniformConstant);
        sampler_code[2u] = sampler->variable_spirv_id;
        sampler_code[3u] = SpvStorageClassUniformConstant;

        spirv_emit_descriptor_set (&context, sampler->variable_spirv_id, (spirv_size_t) sampler->set);
        spirv_emit_binding (&context, sampler->variable_spirv_id, sampler->binding);
        spirv_generate_op_name (&context, sampler->variable_spirv_id, sampler->name);
        sampler = sampler->next;
    }

    struct compiler_instance_image_node_t *image = instance->first_image;
    while (image)
    {
        if (!image->used)
        {
            image = image->next;
            continue;
        }

        image->variable_spirv_id = context.current_bound;
        ++context.current_bound;

        struct compiler_instance_type_definition_t type_definition = {
            .class = COMPILER_INSTANCE_TYPE_CLASS_IMAGE,
            .image_type = image->type,
            .array_size_runtime = KAN_FALSE,
            .array_dimensions_count = image->array_size > 1u ? 1u : 0u,
            .array_dimensions = &image->array_size,
        };

        spirv_size_t *image_code = spirv_new_instruction (&context, &context.global_variable_section, 4u);
        image_code[0u] |= SpvOpCodeMask & SpvOpVariable;
        image_code[1u] = spirv_get_or_create_pointer_type (
            &context, spirv_find_or_generate_object_type (&context, &type_definition, 0u),
            SpvStorageClassUniformConstant);
        image_code[2u] = image->variable_spirv_id;
        image_code[3u] = SpvStorageClassUniformConstant;

        spirv_emit_descriptor_set (&context, image->variable_spirv_id, (spirv_size_t) image->set);
        spirv_emit_binding (&context, image->variable_spirv_id, image->binding);
        spirv_generate_op_name (&context, image->variable_spirv_id, image->name);
        image = image->next;
    }

    // Function first pass: just generate ids (needed in case of recursion).
    struct compiler_instance_function_node_t *function_node = instance->first_function;

    while (function_node)
    {
        function_node->spirv_id = context.current_bound;
        ++context.current_bound;
        function_node->spirv_function_type = spirv_find_or_generate_function_type (&context, function_node);
        function_node = function_node->next;
    }

    // Function second pass: now we can truly generate functions.
    function_node = instance->first_function;

    while (function_node)
    {
        spirv_emit_function (&context, function_node);
        function_node = function_node->next;
    }

    return spirv_finalize_generation_context (&context, output);
}
