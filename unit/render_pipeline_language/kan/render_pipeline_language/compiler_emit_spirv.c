// It seems like we need this define for M_PI for MSVC.
#define _USE_MATH_DEFINES
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
    struct inbuilt_vector_type_t *base_type_if_vector;
    struct inbuilt_matrix_type_t *base_type_if_matrix;
    struct compiler_instance_struct_node_t *base_type_if_struct;
    kan_bool_t runtime_size;
    kan_instance_size_t dimensions_count;
    kan_rpl_size_t *dimensions;
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

struct spirv_generation_integer_constant_t
{
    struct spirv_generation_integer_constant_t *next;
    spirv_size_t spirv_id;
    spirv_signed_literal_t value;
};

struct spirv_generation_floating_constant_t
{
    struct spirv_generation_floating_constant_t *next;
    spirv_size_t spirv_id;
    float value;
};

struct spirv_known_pointer_type_t
{
    struct spirv_known_pointer_type_t *next;
    spirv_size_t source_type_id;
    spirv_size_t pointer_type_id;
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

    struct spirv_generation_integer_constant_t *first_integer_constant;
    struct spirv_generation_floating_constant_t *first_floating_constant;

    struct spirv_known_pointer_type_t *first_known_input_pointer;
    struct spirv_known_pointer_type_t *first_known_output_pointer;
    struct spirv_known_pointer_type_t *first_known_uniform_pointer;
    struct spirv_known_pointer_type_t *first_known_uniform_constant_pointer;
    struct spirv_known_pointer_type_t *first_known_storage_buffer_pointer;
    struct spirv_known_pointer_type_t *first_known_function_pointer;
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
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_FLOAT, "f1");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_INTEGER, "i1");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_F2, "f2");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_F3, "f3");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_F4, "f4");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_I2, "i2");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_I3, "i3");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_I4, "i4");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_F3X3, "f3x3");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_F4X4, "f4x4");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_COMMON_SAMPLER, "common_sampler_type");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_SAMPLER_2D_IMAGE, "sampler_2d_image");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_SAMPLER_2D, "sampler_2d");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_SAMPLER_2D_POINTER, "sampler_2d");

    spirv_size_t *code = spirv_new_instruction (context, &context->base_type_section, 2u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVoid;
    code[1u] = SPIRV_FIXED_ID_TYPE_VOID;

    code = spirv_new_instruction (context, &context->base_type_section, 2u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeBool;
    code[1u] = SPIRV_FIXED_ID_TYPE_BOOLEAN;

    code = spirv_new_instruction (context, &context->base_type_section, 3u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeFloat;
    code[1u] = SPIRV_FIXED_ID_TYPE_FLOAT;
    code[2u] = 32u;

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_FLOAT_INPUT_POINTER, SPIRV_FIXED_ID_TYPE_FLOAT,
                                                    SpvStorageClassInput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_FLOAT_OUTPUT_POINTER, SPIRV_FIXED_ID_TYPE_FLOAT,
                                                    SpvStorageClassOutput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_FLOAT_FUNCTION_POINTER,
                                                    SPIRV_FIXED_ID_TYPE_FLOAT, SpvStorageClassFunction);

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeInt;
    code[1u] = SPIRV_FIXED_ID_TYPE_INTEGER;
    code[2u] = 32u;
    code[3u] = 1u; // Signed.

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_INTEGER_INPUT_POINTER,
                                                    SPIRV_FIXED_ID_TYPE_INTEGER, SpvStorageClassInput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_INTEGER_OUTPUT_POINTER,
                                                    SPIRV_FIXED_ID_TYPE_INTEGER, SpvStorageClassOutput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_INTEGER_FUNCTION_POINTER,
                                                    SPIRV_FIXED_ID_TYPE_INTEGER, SpvStorageClassFunction);

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_F2;
    code[2u] = SPIRV_FIXED_ID_TYPE_FLOAT;
    code[3u] = 2u;

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_F2_INPUT_POINTER, SPIRV_FIXED_ID_TYPE_F2,
                                                    SpvStorageClassInput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_F2_OUTPUT_POINTER, SPIRV_FIXED_ID_TYPE_F2,
                                                    SpvStorageClassOutput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_F2_FUNCTION_POINTER, SPIRV_FIXED_ID_TYPE_F2,
                                                    SpvStorageClassFunction);

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_F3;
    code[2u] = SPIRV_FIXED_ID_TYPE_FLOAT;
    code[3u] = 3u;

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_F3_INPUT_POINTER, SPIRV_FIXED_ID_TYPE_F3,
                                                    SpvStorageClassInput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_F3_OUTPUT_POINTER, SPIRV_FIXED_ID_TYPE_F3,
                                                    SpvStorageClassOutput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_F3_FUNCTION_POINTER, SPIRV_FIXED_ID_TYPE_F3,
                                                    SpvStorageClassFunction);

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_F4;
    code[2u] = SPIRV_FIXED_ID_TYPE_FLOAT;
    code[3u] = 4u;

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_F4_INPUT_POINTER, SPIRV_FIXED_ID_TYPE_F4,
                                                    SpvStorageClassInput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_F4_OUTPUT_POINTER, SPIRV_FIXED_ID_TYPE_F4,
                                                    SpvStorageClassOutput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_F4_FUNCTION_POINTER, SPIRV_FIXED_ID_TYPE_F4,
                                                    SpvStorageClassFunction);

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_I2;
    code[2u] = SPIRV_FIXED_ID_TYPE_INTEGER;
    code[3u] = 2u;

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_I2_INPUT_POINTER, SPIRV_FIXED_ID_TYPE_I2,
                                                    SpvStorageClassInput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_I2_OUTPUT_POINTER, SPIRV_FIXED_ID_TYPE_I2,
                                                    SpvStorageClassOutput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_I2_FUNCTION_POINTER, SPIRV_FIXED_ID_TYPE_I2,
                                                    SpvStorageClassFunction);

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_I3;
    code[2u] = SPIRV_FIXED_ID_TYPE_INTEGER;
    code[3u] = 3u;

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_I3_INPUT_POINTER, SPIRV_FIXED_ID_TYPE_I3,
                                                    SpvStorageClassInput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_I3_OUTPUT_POINTER, SPIRV_FIXED_ID_TYPE_I3,
                                                    SpvStorageClassOutput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_I3_FUNCTION_POINTER, SPIRV_FIXED_ID_TYPE_I3,
                                                    SpvStorageClassFunction);

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_I4;
    code[2u] = SPIRV_FIXED_ID_TYPE_INTEGER;
    code[3u] = 4u;

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_I4_INPUT_POINTER, SPIRV_FIXED_ID_TYPE_I4,
                                                    SpvStorageClassInput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_I4_OUTPUT_POINTER, SPIRV_FIXED_ID_TYPE_I4,
                                                    SpvStorageClassOutput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_I4_FUNCTION_POINTER, SPIRV_FIXED_ID_TYPE_I4,
                                                    SpvStorageClassFunction);

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeMatrix;
    code[1u] = SPIRV_FIXED_ID_TYPE_F3X3;
    code[2u] = SPIRV_FIXED_ID_TYPE_F3;
    code[3u] = 3u;

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_F3X3_INPUT_POINTER, SPIRV_FIXED_ID_TYPE_F3X3,
                                                    SpvStorageClassInput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_F3X3_OUTPUT_POINTER, SPIRV_FIXED_ID_TYPE_F3X3,
                                                    SpvStorageClassOutput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_F3X3_FUNCTION_POINTER, SPIRV_FIXED_ID_TYPE_F3X3,
                                                    SpvStorageClassFunction);

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeMatrix;
    code[1u] = SPIRV_FIXED_ID_TYPE_F4X4;
    code[2u] = SPIRV_FIXED_ID_TYPE_F4;
    code[3u] = 4u;

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_F4X4_INPUT_POINTER, SPIRV_FIXED_ID_TYPE_F4X4,
                                                    SpvStorageClassInput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_F4X4_OUTPUT_POINTER, SPIRV_FIXED_ID_TYPE_F4X4,
                                                    SpvStorageClassOutput);

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_F4X4_FUNCTION_POINTER, SPIRV_FIXED_ID_TYPE_F4X4,
                                                    SpvStorageClassFunction);

    code = spirv_new_instruction (context, &context->base_type_section, 2u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeSampler;
    code[1u] = SPIRV_FIXED_ID_TYPE_COMMON_SAMPLER;

    code = spirv_new_instruction (context, &context->base_type_section, 9u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeImage;
    code[1u] = SPIRV_FIXED_ID_TYPE_SAMPLER_2D_IMAGE;
    code[2u] = STATICS.type_f1.spirv_id;
    code[3u] = SpvDim2D;
    code[4u] = 0u;
    code[5u] = 0u;
    code[6u] = 0u;
    code[7u] = 1u;
    code[8u] = SpvImageFormatUnknown;

    code = spirv_new_instruction (context, &context->base_type_section, 3u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeSampledImage;
    code[1u] = SPIRV_FIXED_ID_TYPE_SAMPLER_2D;
    code[2u] = SPIRV_FIXED_ID_TYPE_SAMPLER_2D_IMAGE;

    spirv_register_and_generate_known_pointer_type (context, &context->base_type_section,
                                                    SPIRV_FIXED_ID_TYPE_SAMPLER_2D_POINTER,
                                                    SPIRV_FIXED_ID_TYPE_SAMPLER_2D, SpvStorageClassUniformConstant);
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

    context->first_integer_constant = NULL;
    context->first_floating_constant = NULL;
    context->first_builtin = NULL;

    context->first_known_input_pointer = NULL;
    context->first_known_output_pointer = NULL;
    context->first_known_uniform_pointer = NULL;
    context->first_known_uniform_constant_pointer = NULL;
    context->first_known_storage_buffer_pointer = NULL;
    context->first_known_function_pointer = NULL;
    spirv_generate_standard_types (context);
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

static inline kan_bool_t spirv_is_buffer_shared_across_invocations (struct compiler_instance_buffer_node_t *buffer)
{
    switch (buffer->type)
    {
    case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
    case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
    case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
    case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
        return KAN_FALSE;

    case KAN_RPL_BUFFER_TYPE_UNIFORM:
    case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
        return KAN_TRUE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
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
        spirv_size_t *op_shader_capability = spirv_new_instruction (context, &base_section, 2u);
        *op_shader_capability |= SpvOpCodeMask & SpvOpCapability;
        *(op_shader_capability + 1u) = SpvCapabilityShader;
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
        struct compiler_instance_buffer_access_node_t *buffer_access = function->first_buffer_access;

        while (buffer_access)
        {
            if (!spirv_is_buffer_shared_across_invocations (buffer_access->buffer))
            {
                if (buffer_access->buffer->first_flattened_declaration)
                {
                    struct compiler_instance_buffer_flattened_declaration_t *declaration =
                        buffer_access->buffer->first_flattened_declaration;
                    while (declaration)
                    {
                        ++accesses_count;
                        declaration = declaration->next;
                    }
                }
                else
                {
                    ++accesses_count;
                }
            }

            buffer_access = buffer_access->next;
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
        buffer_access = function->first_buffer_access;

        while (buffer_access)
        {
            if (!spirv_is_buffer_shared_across_invocations (buffer_access->buffer))
            {
                if (buffer_access->buffer->first_flattened_declaration)
                {
                    struct compiler_instance_buffer_flattened_declaration_t *declaration =
                        buffer_access->buffer->first_flattened_declaration;

                    while (declaration)
                    {
                        *access_output =
                            buffer_access->used_as_output ? declaration->spirv_id_output : declaration->spirv_id_input;
                        ++access_output;
                        declaration = declaration->next;
                    }
                }
                else
                {
                    *access_output = buffer_access->buffer->structured_variable_spirv_id;
                    ++access_output;
                }
            }

            buffer_access = buffer_access->next;
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

static spirv_size_t spirv_request_i1_constant (struct spirv_generation_context_t *context, spirv_signed_literal_t value)
{
    struct spirv_generation_integer_constant_t *existent_constant = context->first_integer_constant;
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

    spirv_size_t *constant_code = spirv_new_instruction (context, &context->base_type_section, 4u);
    constant_code[0u] |= SpvOpCodeMask & SpvOpConstant;
    constant_code[1u] = STATICS.type_i1.spirv_id;
    constant_code[2u] = constant_id;
    *(spirv_signed_literal_t *) &constant_code[3u] = (spirv_signed_literal_t) value;

    struct spirv_generation_integer_constant_t *new_constant = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &context->temporary_allocator, struct spirv_generation_integer_constant_t);

    new_constant->next = context->first_integer_constant;
    context->first_integer_constant = new_constant;
    new_constant->spirv_id = constant_id;
    new_constant->value = value;
    return constant_id;
}

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

    spirv_size_t *constant_code = spirv_new_instruction (context, &context->base_type_section, 4u);
    constant_code[0u] |= SpvOpCodeMask & SpvOpConstant;
    constant_code[1u] = STATICS.type_f1.spirv_id;
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

static spirv_size_t spirv_find_or_generate_variable_type (struct spirv_generation_context_t *context,
                                                          struct compiler_instance_full_type_definition_t *type,
                                                          kan_loop_size_t start_dimension_index)
{
    if ((!type->array_size_runtime && start_dimension_index == type->array_dimensions_count) ||
        (type->array_size_runtime && start_dimension_index == 1u))
    {
        if (type->if_vector)
        {
            return type->if_vector->spirv_id;
        }
        else if (type->if_matrix)
        {
            return type->if_matrix->spirv_id;
        }
        else if (type->if_struct)
        {
            return type->if_struct->spirv_id_value;
        }

        KAN_ASSERT (KAN_FALSE)
    }

    struct spirv_generation_array_type_t *array_type = context->first_generated_array_type;
    while (array_type)
    {
        if (array_type->base_type_if_vector == type->if_vector && array_type->base_type_if_matrix == type->if_matrix &&
            array_type->base_type_if_struct == type->if_struct &&
            array_type->runtime_size == type->array_size_runtime &&
            array_type->dimensions_count == type->array_dimensions_count - start_dimension_index &&
            memcmp (array_type->dimensions, &type->array_dimensions[start_dimension_index],
                    array_type->dimensions_count * sizeof (kan_rpl_size_t)) == 0)
        {
            return array_type->spirv_id;
        }

        array_type = array_type->next;
    }

    const spirv_size_t base_type_id = spirv_find_or_generate_variable_type (context, type, start_dimension_index + 1u);
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
            spirv_request_i1_constant (context, (spirv_size_t) type->array_dimensions[start_dimension_index]);

        spirv_size_t *dimension_type_code = spirv_new_instruction (context, &context->higher_type_section, 4u);
        dimension_type_code[0u] |= SpvOpCodeMask & SpvOpTypeArray;
        dimension_type_code[1u] = array_type_id;
        dimension_type_code[2u] = base_type_id;
        dimension_type_code[3u] = constant_id;
    }

    kan_instance_size_t base_size = 0u;
    kan_instance_size_t base_alignment = 0u;
    calculate_full_type_definition_size_and_alignment (type, start_dimension_index + 1u, &base_size, &base_alignment);

    spirv_size_t *array_stride_code = spirv_new_instruction (context, &context->decoration_section, 4u);
    array_stride_code[0u] |= SpvOpCodeMask & SpvOpDecorate;
    array_stride_code[1u] = array_type_id;
    array_stride_code[2u] = SpvDecorationArrayStride;
    array_stride_code[3u] = (spirv_size_t) base_size;

    struct spirv_generation_array_type_t *new_array_type =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&context->temporary_allocator, struct spirv_generation_array_type_t);

    new_array_type->next = context->first_generated_array_type;
    context->first_generated_array_type = new_array_type;
    new_array_type->spirv_id = array_type_id;
    new_array_type->base_type_if_vector = type->if_vector;
    new_array_type->base_type_if_matrix = type->if_matrix;
    new_array_type->base_type_if_struct = type->if_struct;
    new_array_type->runtime_size = type->array_size_runtime;
    new_array_type->dimensions_count = type->array_dimensions_count - start_dimension_index;
    new_array_type->dimensions = &type->array_dimensions[start_dimension_index];

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
            spirv_size_t field_type_id = spirv_find_or_generate_variable_type (context, &field->variable.type, 0u);
            fields[field_index] = field_type_id;

            spirv_size_t *offset_code = spirv_new_instruction (context, &context->decoration_section, 5u);
            offset_code[0u] |= SpvOpCodeMask & SpvOpMemberDecorate;
            offset_code[1u] = struct_id;
            offset_code[2u] = (spirv_size_t) field_index;
            offset_code[3u] = SpvDecorationOffset;
            offset_code[4u] = (spirv_size_t) field->offset;

            if (field->variable.type.if_matrix)
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
                matrix_stride_code[4u] = (spirv_size_t) inbuilt_type_item_size[field->variable.type.if_matrix->item] *
                                         field->variable.type.if_matrix->rows;
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

static inline void spirv_emit_flattened_input_variable (
    struct spirv_generation_context_t *context,
    struct compiler_instance_buffer_flattened_declaration_t *declaration,
    kan_bool_t context_needs_flat_decoration)
{
    declaration->spirv_id_input = context->current_bound;
    ++context->current_bound;

    spirv_size_t *variable_code = spirv_new_instruction (context, &context->global_variable_section, 4u);
    variable_code[0u] |= SpvOpCodeMask & SpvOpVariable;

    if (declaration->source_declaration->variable.type.if_vector)
    {
        variable_code[1u] = declaration->source_declaration->variable.type.if_vector->spirv_id_input_pointer;
    }
    else if (declaration->source_declaration->variable.type.if_matrix)
    {
        variable_code[1u] = declaration->source_declaration->variable.type.if_matrix->spirv_id_input_pointer;
    }
    else
    {
        KAN_ASSERT (KAN_FALSE)
    }

    variable_code[2u] = declaration->spirv_id_input;
    variable_code[3u] = SpvStorageClassInput;

    spirv_emit_location (context, declaration->spirv_id_input, declaration->location);
    spirv_generate_op_name (context, declaration->spirv_id_input, declaration->readable_name);

    if (context_needs_flat_decoration && (declaration->source_declaration->variable.type.if_vector ||
                                          declaration->source_declaration->variable.type.if_matrix))
    {
        enum inbuilt_type_item_t item_type;
        if (declaration->source_declaration->variable.type.if_vector)
        {
            item_type = declaration->source_declaration->variable.type.if_vector->item;
        }
        else
        {
            item_type = declaration->source_declaration->variable.type.if_matrix->item;
        }

        switch (item_type)
        {
        case INBUILT_TYPE_ITEM_FLOAT:
            break;

        case INBUILT_TYPE_ITEM_INTEGER:
        {
            spirv_size_t *decoration_code = spirv_new_instruction (context, &context->decoration_section, 3u);
            decoration_code[0u] |= SpvOpCodeMask & SpvOpDecorate;
            decoration_code[1u] = declaration->spirv_id_input;
            decoration_code[2u] = SpvDecorationFlat;
            break;
        }
        }
    }
}

static inline void spirv_emit_flattened_output_variable (
    struct spirv_generation_context_t *context, struct compiler_instance_buffer_flattened_declaration_t *declaration)
{
    declaration->spirv_id_output = context->current_bound;
    ++context->current_bound;

    spirv_size_t *variable_code = spirv_new_instruction (context, &context->global_variable_section, 4u);
    variable_code[0u] |= SpvOpCodeMask & SpvOpVariable;

    if (declaration->source_declaration->variable.type.if_vector)
    {
        variable_code[1u] = declaration->source_declaration->variable.type.if_vector->spirv_id_output_pointer;
    }
    else if (declaration->source_declaration->variable.type.if_matrix)
    {
        variable_code[1u] = declaration->source_declaration->variable.type.if_matrix->spirv_id_output_pointer;
    }
    else
    {
        KAN_ASSERT (KAN_FALSE)
    }

    variable_code[2u] = declaration->spirv_id_output;
    variable_code[3u] = SpvStorageClassOutput;

    spirv_emit_location (context, declaration->spirv_id_output, declaration->location);
    spirv_generate_op_name (context, declaration->spirv_id_output, declaration->readable_name);
}

static struct spirv_generation_function_type_t *spirv_find_or_generate_function_type (
    struct spirv_generation_context_t *context, struct compiler_instance_function_node_t *function)
{
    spirv_size_t return_type;
    if (function->return_type_if_vector)
    {
        return_type = function->return_type_if_vector->spirv_id;
    }
    else if (function->return_type_if_matrix)
    {
        return_type = function->return_type_if_matrix->spirv_id;
    }
    else if (function->return_type_if_struct)
    {
        return_type = function->return_type_if_struct->spirv_id_value;
    }
    else
    {
        return_type = SPIRV_FIXED_ID_TYPE_VOID;
    }

    kan_loop_size_t argument_count = 0u;
    struct compiler_instance_declaration_node_t *argument = function->first_argument;

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
            argument_types[argument_index] = spirv_get_or_create_pointer_type (
                context, spirv_find_or_generate_variable_type (context, &argument->variable.type, 0u),
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
        *can_be_out_of_bounds = KAN_TRUE;
        *root_expression = top_expression;

        switch (top_expression->structured_buffer_reference->type)
        {
        case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
        case KAN_RPL_BUFFER_TYPE_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
        case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
        case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
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
            spirv_size_t constant_id = spirv_request_i1_constant (
                context, (spirv_signed_literal_t) top_expression->structured_access.access_chain_indices[index]);

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

        *output = index_id;
        ++output;
        return output;
    }
    // Instanced buffers generate hidden instance data access.
    else if (top_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE)
    {
        switch (top_expression->structured_buffer_reference->type)
        {
        case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
        case KAN_RPL_BUFFER_TYPE_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
        case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
        case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
            return output;
        }
    }

    return output;
}

static inline SpvStorageClass spirv_get_structured_buffer_storage_class (struct compiler_instance_buffer_node_t *buffer)
{
    switch (buffer->type)
    {
    case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
    case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
    case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
    case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
        KAN_ASSERT (KAN_FALSE)
        return (SpvStorageClass) SPIRV_FIXED_ID_INVALID;

    case KAN_RPL_BUFFER_TYPE_UNIFORM:
        return SpvStorageClassUniform;

    case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
        return SpvStorageClassStorageBuffer;
    }

    KAN_ASSERT (KAN_FALSE)
    return (SpvStorageClass) SPIRV_FIXED_ID_INVALID;
}

static spirv_size_t spirv_use_temporary_variable (struct spirv_generation_context_t *context,
                                                  struct spirv_generation_function_node_t *function,
                                                  struct compiler_instance_expression_output_type_t *required_type)
{
    // We do not expect temporary variables to be arrays or booleans.
    // If they are, then something is wrong with the resolve.
    KAN_ASSERT (required_type->type.array_dimensions_count == 0u)
    KAN_ASSERT (!required_type->type.array_size_runtime)
    KAN_ASSERT (!required_type->boolean)

    spirv_size_t required_type_id;
    if (required_type->type.if_vector)
    {
        required_type_id = required_type->type.if_vector->spirv_id_function_pointer;
    }
    else if (required_type->type.if_matrix)
    {
        required_type_id = required_type->type.if_matrix->spirv_id_function_pointer;
    }
    else if (required_type->type.if_struct)
    {
        required_type_id = required_type->type.if_struct->spirv_id_function_pointer;
    }
    else
    {
        KAN_ASSERT (KAN_FALSE)
        required_type_id = (spirv_size_t) SPIRV_FIXED_ID_INVALID;
    }

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

    spirv_size_t result_value_type = spirv_find_or_generate_variable_type (context, &top_expression->output.type, 0u);
    spirv_size_t result_pointer_type = (spirv_size_t) SPIRV_FIXED_ID_INVALID;

    switch (root_expression->type)
    {
    case COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE:
        result_pointer_type = spirv_get_or_create_pointer_type (
            context, result_value_type,
            spirv_get_structured_buffer_storage_class (root_expression->structured_buffer_reference));
        break;

    case COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_REFERENCE:
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
    case COMPILER_INSTANCE_EXPRESSION_TYPE_SAMPLER_CALL:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_CONSTRUCTOR:
        result_pointer_type = spirv_get_or_create_pointer_type (context, result_value_type, SpvStorageClassFunction);
        break;

    case COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ARRAY_INDEX:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_INTEGER_LITERAL:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_FLOATING_LITERAL:
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

    case COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS_INPUT:
        result_pointer_type = spirv_get_or_create_pointer_type (context, result_value_type, SpvStorageClassInput);
        break;

    case COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS_OUTPUT:
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

#define SPIRV_EMIT_VECTOR_ARITHMETIC(SUFFIX, FLOAT_OP, INTEGER_OP)                                                     \
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
            code[1u] = type->spirv_id;                                                                                 \
            code[2u] = result_id;                                                                                      \
            code[3u] = left;                                                                                           \
            code[4u] = right;                                                                                          \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        case INBUILT_TYPE_ITEM_INTEGER:                                                                                \
        {                                                                                                              \
            spirv_size_t *code = spirv_new_instruction (context, section, 5u);                                         \
            code[0u] |= SpvOpCodeMask & INTEGER_OP;                                                                    \
            code[1u] = type->spirv_id;                                                                                 \
            code[2u] = result_id;                                                                                      \
            code[3u] = left;                                                                                           \
            code[4u] = right;                                                                                          \
            break;                                                                                                     \
        }                                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        return result_id;                                                                                              \
    }

SPIRV_EMIT_VECTOR_ARITHMETIC (add, SpvOpFAdd, SpvOpIAdd)
SPIRV_EMIT_VECTOR_ARITHMETIC (sub, SpvOpFSub, SpvOpISub)
SPIRV_EMIT_VECTOR_ARITHMETIC (mul, SpvOpFMul, SpvOpIMul)
SPIRV_EMIT_VECTOR_ARITHMETIC (div, SpvOpFDiv, SpvOpSDiv)
#undef SPIRV_EMIT_VECTOR_ARITHMETIC

#define SPIRV_EMIT_MATRIX_ARITHMETIC(SUFFIX)                                                                           \
    static inline spirv_size_t spirv_emit_matrix_##SUFFIX (                                                            \
        struct spirv_generation_context_t *context, struct spirv_arbitrary_instruction_section_t *section,             \
        struct inbuilt_matrix_type_t *type, spirv_size_t left, spirv_size_t right)                                     \
    {                                                                                                                  \
        spirv_size_t column_result_ids[4u];                                                                            \
        KAN_ASSERT (type->columns <= 4u)                                                                               \
        struct inbuilt_vector_type_t *column_type = NULL;                                                              \
                                                                                                                       \
        switch (type->item)                                                                                            \
        {                                                                                                              \
        case INBUILT_TYPE_ITEM_FLOAT:                                                                                  \
            column_type = STATICS.floating_vector_types[type->rows - 1u];                                              \
            break;                                                                                                     \
                                                                                                                       \
        case INBUILT_TYPE_ITEM_INTEGER:                                                                                \
            column_type = STATICS.integer_vector_types[type->rows - 1u];                                               \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        for (kan_loop_size_t column_index = 0u; column_index < type->columns; ++column_index)                          \
        {                                                                                                              \
            spirv_size_t left_extract_result = context->current_bound;                                                 \
            ++context->current_bound;                                                                                  \
                                                                                                                       \
            spirv_size_t *left_extract = spirv_new_instruction (context, section, 5u);                                 \
            left_extract[0u] |= SpvOpCodeMask & SpvOpCompositeExtract;                                                 \
            left_extract[1u] = column_type->spirv_id;                                                                  \
            left_extract[2u] = left_extract_result;                                                                    \
            left_extract[3u] = left;                                                                                   \
            left_extract[4u] = (spirv_size_t) column_index;                                                            \
                                                                                                                       \
            spirv_size_t right_extract_result = context->current_bound;                                                \
            ++context->current_bound;                                                                                  \
                                                                                                                       \
            spirv_size_t *right_extract = spirv_new_instruction (context, section, 5u);                                \
            right_extract[0u] |= SpvOpCodeMask & SpvOpCompositeExtract;                                                \
            right_extract[1u] = column_type->spirv_id;                                                                 \
            right_extract[2u] = right_extract_result;                                                                  \
            right_extract[3u] = right;                                                                                 \
            right_extract[4u] = (spirv_size_t) column_index;                                                           \
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
        construct[1u] = type->spirv_id;                                                                                \
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
                arguments_should_be_pointers &&
                // Special case for flattened input variables: they're technically pointers, but they have different
                // storage type, therefore they should be copied into function variable first.
                (argument->expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS_INPUT ||
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

    spirv_size_t result_type_id;
    if (expression->function_call.function->return_type_if_vector)
    {
        result_type_id = expression->function_call.function->return_type_if_vector->spirv_id;
    }
    else if (expression->function_call.function->return_type_if_matrix)
    {
        result_type_id = expression->function_call.function->return_type_if_matrix->spirv_id;
    }
    else if (expression->function_call.function->return_type_if_struct)
    {
        result_type_id = expression->function_call.function->return_type_if_struct->spirv_id_value;
    }
    else
    {
        result_type_id = (spirv_size_t) SPIRV_FIXED_ID_TYPE_VOID;
    }

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

        const spirv_size_t position_builtin = spirv_request_builtin (
            context, function, SpvBuiltInPosition, SpvStorageClassOutput, STATICS.type_f4.spirv_id_output_pointer);

        spirv_emit_store (context, *current_block, position_builtin, operand_id);
        // Just a store operation, has no return.
        return (spirv_size_t) SPIRV_FIXED_ID_INVALID;
    }
    else if (expression->function_call.function == &STATICS.builtin_pi)
    {
        return spirv_request_f1_constant (context, (float) M_PI);
    }
    else if (expression->function_call.function == &STATICS.builtin_i1_to_f1 ||
             expression->function_call.function == &STATICS.builtin_i2_to_f2 ||
             expression->function_call.function == &STATICS.builtin_i3_to_f3 ||
             expression->function_call.function == &STATICS.builtin_i4_to_f4)
    {
        spirv_size_t operand_id = spirv_emit_expression (
            context, function, current_block, expression->function_call.first_argument->expression, KAN_FALSE);

        spirv_size_t result_id = context->current_bound;
        ++context->current_bound;

        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 4u);
        code[0u] |= SpvOpCodeMask & SpvOpConvertSToF;
        code[1u] = expression->function_call.function->return_type_if_vector->spirv_id;
        code[2u] = result_id;
        code[3u] = operand_id;

        return result_id;
    }
    else if (expression->function_call.function == &STATICS.builtin_f1_to_i1 ||
             expression->function_call.function == &STATICS.builtin_f2_to_i2 ||
             expression->function_call.function == &STATICS.builtin_f3_to_i3 ||
             expression->function_call.function == &STATICS.builtin_f4_to_i4)
    {
        spirv_size_t operand_id = spirv_emit_expression (
            context, function, current_block, expression->function_call.first_argument->expression, KAN_FALSE);

        spirv_size_t result_id = context->current_bound;
        ++context->current_bound;

        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 4u);
        code[0u] |= SpvOpCodeMask & SpvOpConvertFToS;
        code[1u] = expression->function_call.function->return_type_if_vector->spirv_id;
        code[2u] = result_id;
        code[3u] = operand_id;

        return result_id;
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
        code[1u] = expression->function_call.function->return_type_if_matrix->spirv_id;
        code[2u] = result_id;
        code[3u] = operand_id;

        return result_id;
    }
    else if (expression->function_call.function == &STATICS.builtin_dot_f1 ||
             expression->function_call.function == &STATICS.builtin_dot_f2 ||
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
        code[1u] = expression->function_call.function->return_type_if_vector->spirv_id;
        code[2u] = result_id;
        code[3u] = left_operand_id;
        code[4u] = right_operand_id;

        return result_id;
    }
    else if (expression->function_call.function == &STATICS.builtin_expand_f3_to_f4)
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
        code[0u] |= SpvOpCodeMask & SpvOpCompositeConstruct;
        code[1u] = expression->function_call.function->return_type_if_vector->spirv_id;
        code[2u] = result_id;
        code[3u] = left_operand_id;
        code[4u] = right_operand_id;

        return result_id;
    }
    else if (expression->function_call.function == &STATICS.builtin_crop_f4_to_f3)
    {
        struct compiler_instance_expression_list_item_t *argument = expression->function_call.first_argument;
        spirv_size_t operand_id =
            spirv_emit_expression (context, function, current_block, argument->expression, KAN_FALSE);

        spirv_size_t result_id = context->current_bound;
        ++context->current_bound;

        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 8u);
        code[0u] |= SpvOpCodeMask & SpvOpVectorShuffle;
        code[1u] = expression->function_call.function->return_type_if_vector->spirv_id;
        code[2u] = result_id;
        code[3u] = operand_id;
        code[4u] = operand_id;
        code[5u] = 0u;
        code[6u] = 1u;
        code[7u] = 2u;

        return result_id;
    }
    else if (expression->function_call.function == &STATICS.builtin_crop_f4x4_to_f3x3)
    {
        struct compiler_instance_expression_list_item_t *argument = expression->function_call.first_argument;
        spirv_size_t operand_id =
            spirv_emit_expression (context, function, current_block, argument->expression, KAN_FALSE);

        spirv_size_t column[3u];
        for (kan_loop_size_t index = 0u; index < 3u; ++index)
        {
            spirv_size_t extracted_id = context->current_bound;
            ++context->current_bound;

            column[index] = context->current_bound;
            ++context->current_bound;

            spirv_size_t *extract_code = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
            extract_code[0u] |= SpvOpCodeMask & SpvOpCompositeExtract;
            extract_code[1u] = STATICS.type_f4.spirv_id;
            extract_code[2u] = extracted_id;
            extract_code[3u] = operand_id;
            extract_code[4u] = (spirv_size_t) index;

            spirv_size_t *shuffle_code = spirv_new_instruction (context, &(*current_block)->code_section, 8u);
            shuffle_code[0u] |= SpvOpCodeMask & SpvOpVectorShuffle;
            shuffle_code[1u] = STATICS.type_f3.spirv_id;
            shuffle_code[2u] = column[index];
            shuffle_code[3u] = extracted_id;
            shuffle_code[4u] = extracted_id;
            shuffle_code[5u] = 0u;
            shuffle_code[6u] = 1u;
            shuffle_code[7u] = 2u;
        }

        spirv_size_t result_id = context->current_bound;
        ++context->current_bound;

        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 6u);
        code[0u] |= SpvOpCodeMask & SpvOpCompositeConstruct;
        code[1u] = STATICS.type_f3x3.spirv_id;
        code[2u] = result_id;
        code[3u] = column[0u];
        code[4u] = column[1u];
        code[5u] = column[2u];
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

    case COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_REFERENCE:
        if (result_should_be_pointer)
        {
            return expression->variable_reference->spirv_id;
        }

        return spirv_request_load (context, *current_block,
                                   spirv_find_or_generate_variable_type (context, &expression->output.type, 0u),
                                   expression->variable_reference->spirv_id, KAN_TRUE);

    case COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS:
        // Sometimes access chains can be replaced by composite extracts (for better performance),
        // but we do not support it right now.
        return spirv_emit_access_chain (context, function, current_block, expression, result_should_be_pointer);

    case COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS_INPUT:
        if (result_should_be_pointer)
        {
            return expression->flattened_buffer_access->spirv_id_input;
        }

        return spirv_request_load (context, *current_block,
                                   spirv_find_or_generate_variable_type (context, &expression->output.type, 0u),
                                   expression->flattened_buffer_access->spirv_id_input, KAN_TRUE);

    case COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS_OUTPUT:
        if (result_should_be_pointer)
        {
            return expression->flattened_buffer_access->spirv_id_output;
        }

        return spirv_request_load (context, *current_block,
                                   spirv_find_or_generate_variable_type (context, &expression->output.type, 0u),
                                   expression->flattened_buffer_access->spirv_id_output, KAN_TRUE);

#define WRAP_OPERATION_RESULT_IF_NEEDED                                                                                \
    if (result_should_be_pointer)                                                                                      \
    {                                                                                                                  \
        spirv_size_t variable_id = spirv_use_temporary_variable (context, function, &expression->output);              \
        spirv_emit_store (context, *current_block, variable_id, result_id);                                            \
        result_id = variable_id;                                                                                       \
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_INTEGER_LITERAL:
    {
        spirv_size_t result_id =
            spirv_request_i1_constant (context, (spirv_signed_literal_t) expression->integer_literal);
        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_FLOATING_LITERAL:
    {
        spirv_size_t result_id = spirv_request_f1_constant (context, (float) expression->floating_literal);
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
        spirv_size_t result_id;

        if (expression->output.type.if_vector)
        {
            result_id = spirv_emit_vector_add (context, &(*current_block)->code_section,
                                               expression->output.type.if_vector, left_operand_id, right_operand_id);
        }
        else if (expression->output.type.if_matrix)
        {
            result_id = spirv_emit_matrix_add (context, &(*current_block)->code_section,
                                               expression->output.type.if_matrix, left_operand_id, right_operand_id);
        }
        else
        {
            KAN_ASSERT (KAN_FALSE)
            result_id = (spirv_size_t) SPIRV_FIXED_ID_INVALID;
        }

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_SUBTRACT:
    {
        BINARY_OPERATION_COMMON_PREPARE;
        spirv_size_t result_id;

        if (expression->output.type.if_vector)
        {
            result_id = spirv_emit_vector_sub (context, &(*current_block)->code_section,
                                               expression->output.type.if_vector, left_operand_id, right_operand_id);
        }
        else if (expression->output.type.if_matrix)
        {
            result_id = spirv_emit_matrix_sub (context, &(*current_block)->code_section,
                                               expression->output.type.if_matrix, left_operand_id, right_operand_id);
        }
        else
        {
            KAN_ASSERT (KAN_FALSE)
            result_id = (spirv_size_t) SPIRV_FIXED_ID_INVALID;
        }

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_MULTIPLY:
    {
        BINARY_OPERATION_COMMON_PREPARE;
        spirv_size_t result_id;

        if (expression->binary_operation.left_operand->output.type.if_vector &&
            expression->binary_operation.left_operand->output.type.if_vector ==
                expression->binary_operation.right_operand->output.type.if_vector)
        {
            result_id = spirv_emit_vector_mul (context, &(*current_block)->code_section,
                                               expression->output.type.if_vector, left_operand_id, right_operand_id);
        }
        else if (expression->binary_operation.left_operand->output.type.if_vector &&
                 expression->binary_operation.right_operand->output.type.if_vector &&
                 expression->binary_operation.right_operand->output.type.if_vector->items_count == 1u)
        {
            result_id = context->current_bound;
            ++context->current_bound;

            spirv_size_t *multiply = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
            multiply[0u] |= SpvOpCodeMask & SpvOpVectorTimesScalar;
            multiply[1u] = expression->output.type.if_vector->spirv_id;
            multiply[2u] = result_id;
            multiply[3u] = left_operand_id;
            multiply[4u] = right_operand_id;
        }
        else if (expression->binary_operation.left_operand->output.type.if_matrix &&
                 expression->binary_operation.right_operand->output.type.if_vector)
        {
            result_id = context->current_bound;
            ++context->current_bound;

            if (expression->binary_operation.right_operand->output.type.if_vector->items_count == 1u)
            {
                spirv_size_t *multiply = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
                multiply[0u] |= SpvOpCodeMask & SpvOpMatrixTimesScalar;
                multiply[1u] = expression->output.type.if_matrix->spirv_id;
                multiply[2u] = result_id;
                multiply[3u] = left_operand_id;
                multiply[4u] = right_operand_id;
            }
            else
            {
                spirv_size_t *multiply = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
                multiply[0u] |= SpvOpCodeMask & SpvOpMatrixTimesVector;
                multiply[1u] = expression->output.type.if_vector->spirv_id;
                multiply[2u] = result_id;
                multiply[3u] = left_operand_id;
                multiply[4u] = right_operand_id;
            }
        }
        else if (expression->binary_operation.left_operand->output.type.if_vector &&
                 expression->binary_operation.right_operand->output.type.if_matrix)
        {
            result_id = context->current_bound;
            ++context->current_bound;

            spirv_size_t *multiply = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
            multiply[0u] |= SpvOpCodeMask & SpvOpVectorTimesMatrix;
            multiply[1u] = expression->output.type.if_vector->spirv_id;
            multiply[2u] = result_id;
            multiply[3u] = left_operand_id;
            multiply[4u] = right_operand_id;
        }
        else if (expression->binary_operation.left_operand->output.type.if_matrix &&
                 expression->binary_operation.right_operand->output.type.if_matrix)
        {
            result_id = context->current_bound;
            ++context->current_bound;

            spirv_size_t *multiply = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
            multiply[0u] |= SpvOpCodeMask & SpvOpMatrixTimesMatrix;
            multiply[1u] = expression->output.type.if_matrix->spirv_id;
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

        if (expression->binary_operation.left_operand->output.type.if_vector &&
            expression->binary_operation.left_operand->output.type.if_vector ==
                expression->binary_operation.right_operand->output.type.if_vector)
        {
            result_id = spirv_emit_vector_div (context, &(*current_block)->code_section,
                                               expression->output.type.if_vector, left_operand_id, right_operand_id);
        }
        else if (expression->binary_operation.left_operand->output.type.if_matrix &&
                 expression->binary_operation.left_operand->output.type.if_matrix ==
                     expression->binary_operation.right_operand->output.type.if_matrix)
        {
            result_id = spirv_emit_matrix_div (context, &(*current_block)->code_section,
                                               expression->output.type.if_matrix, left_operand_id, right_operand_id);
        }
        else if (expression->binary_operation.left_operand->output.type.if_vector &&
                 expression->binary_operation.right_operand->output.type.if_vector &&
                 expression->binary_operation.right_operand->output.type.if_vector->items_count == 1u)
        {
            spirv_size_t composite_id = context->current_bound;
            ++context->current_bound;

            spirv_size_t *construct = spirv_new_instruction (
                context, &(*current_block)->code_section,
                3u + expression->binary_operation.left_operand->output.type.if_vector->items_count);
            construct[0u] |= SpvOpCodeMask & SpvOpCompositeConstruct;
            construct[1u] = expression->binary_operation.left_operand->output.type.if_vector->spirv_id;
            construct[2u] = composite_id;

            for (kan_loop_size_t index = 0u;
                 index < expression->binary_operation.left_operand->output.type.if_vector->items_count; ++index)
            {
                construct[3u + index] = right_operand_id;
            }

            result_id = spirv_emit_vector_div (context, &(*current_block)->code_section,
                                               expression->binary_operation.left_operand->output.type.if_vector,
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

        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
        code[0u] |= SpvOpCodeMask & SpvOpSMod;
        code[1u] = expression->output.type.if_vector->spirv_id;
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

#define SCALAR_LOGICAL_OPERATION(WHEN_FLOAT, WHEN_INTEGER)                                                             \
    {                                                                                                                  \
        BINARY_OPERATION_COMMON_PREPARE;                                                                               \
        spirv_size_t result_id = context->current_bound;                                                               \
        ++context->current_bound;                                                                                      \
                                                                                                                       \
        spirv_size_t operation = SpvOpCodeMask;                                                                        \
        switch (expression->binary_operation.left_operand->output.type.if_vector->item)                                \
        {                                                                                                              \
        case INBUILT_TYPE_ITEM_FLOAT:                                                                                  \
            operation = WHEN_FLOAT;                                                                                    \
            break;                                                                                                     \
                                                                                                                       \
        case INBUILT_TYPE_ITEM_INTEGER:                                                                                \
            operation = WHEN_INTEGER;                                                                                  \
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
        SCALAR_LOGICAL_OPERATION (SpvOpFOrdLessThan, SpvOpSLessThan)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_GREATER:
        SCALAR_LOGICAL_OPERATION (SpvOpFOrdGreaterThan, SpvOpSGreaterThan)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_LESS_OR_EQUAL:
        SCALAR_LOGICAL_OPERATION (SpvOpFOrdLessThanEqual, SpvOpSLessThanEqual)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_GREATER_OR_EQUAL:
        SCALAR_LOGICAL_OPERATION (SpvOpFOrdGreaterThanEqual, SpvOpSGreaterThanEqual)

#define TRIVIAL_BITWISE_OPERATION(OPERATION)                                                                           \
    {                                                                                                                  \
        BINARY_OPERATION_COMMON_PREPARE;                                                                               \
        spirv_size_t result_id = context->current_bound;                                                               \
        ++context->current_bound;                                                                                      \
                                                                                                                       \
        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 5u);                     \
        code[0u] |= SpvOpCodeMask & OPERATION;                                                                         \
        code[1u] = expression->output.type.if_vector->spirv_id;                                                        \
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

        if (expression->output.type.if_vector)
        {
            spirv_size_t operation = SpvOpCodeMask;
            switch (expression->output.type.if_vector->item)
            {
            case INBUILT_TYPE_ITEM_FLOAT:
                operation = SpvOpFNegate;
                break;

            case INBUILT_TYPE_ITEM_INTEGER:
                operation = SpvOpSNegate;
                break;
            }

            spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 4u);
            code[0u] |= SpvOpCodeMask & operation;
            code[1u] = expression->output.type.if_vector->spirv_id;
            code[2u] = result_id;
            code[3u] = operand_id;
        }
        else if (expression->output.type.if_matrix)
        {
            spirv_size_t constant_id = (spirv_size_t) SPIRV_FIXED_ID_INVALID;
            switch (expression->output.type.if_matrix->item)
            {
            case INBUILT_TYPE_ITEM_FLOAT:
                constant_id = spirv_request_f1_constant (context, -1.0f);
                break;

            case INBUILT_TYPE_ITEM_INTEGER:
                constant_id = spirv_request_i1_constant (context, (spirv_signed_literal_t) -1);
                break;
            }

            spirv_size_t *multiply = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
            multiply[0u] |= SpvOpCodeMask & SpvOpMatrixTimesScalar;
            multiply[1u] = expression->output.type.if_matrix->spirv_id;
            multiply[2u] = result_id;
            multiply[3u] = operand_id;
            multiply[4u] = constant_id;
        }
        else
        {
            KAN_ASSERT (KAN_FALSE)
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
        code[1u] = expression->output.type.if_vector->spirv_id;
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
            if (expression->function_call.function->return_type_if_vector ||
                expression->function_call.function->return_type_if_matrix ||
                expression->function_call.function->return_type_if_struct)
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

    case COMPILER_INSTANCE_EXPRESSION_TYPE_SAMPLER_CALL:
    {
        // If this fails, then something might be wrong with resolve or signatures.
        KAN_ASSERT (expression->sampler_call.first_argument)
        KAN_ASSERT (!expression->sampler_call.first_argument->next)

        spirv_size_t coordinate_operand = spirv_emit_expression (
            context, function, current_block, expression->sampler_call.first_argument->expression, KAN_FALSE);

        spirv_size_t sampler_type_id = (spirv_size_t) SPIRV_FIXED_ID_INVALID;
        switch (expression->sampler_call.sampler->type)
        {
        case KAN_RPL_SAMPLER_TYPE_2D:
            sampler_type_id = (spirv_size_t) SPIRV_FIXED_ID_TYPE_SAMPLER_2D;
            break;
        }

        spirv_size_t loaded_sampler_id = spirv_request_load (
            context, *current_block, sampler_type_id, expression->sampler_call.sampler->variable_spirv_id, KAN_TRUE);

        spirv_size_t result_id = context->current_bound;
        ++context->current_bound;

        spirv_size_t *code = spirv_new_instruction (context, &(*current_block)->code_section, 5u);
        code[0u] |= SpvOpCodeMask & SpvOpImageSampleImplicitLod;
        code[1u] = STATICS.type_f4.spirv_id;
        code[2u] = result_id;
        code[3u] = loaded_sampler_id;
        code[4u] = coordinate_operand;

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_CONSTRUCTOR:
    {
        kan_instance_size_t argument_count = 0u;
        spirv_size_t *arguments = spirv_gather_call_arguments (
            context, function, current_block, expression->constructor.first_argument, &argument_count, KAN_FALSE);

        spirv_size_t result_id = context->current_bound;
        ++context->current_bound;

        spirv_size_t result_type_id;
        if (expression->constructor.type_if_vector)
        {
            result_type_id = expression->constructor.type_if_vector->spirv_id;
        }
        else if (expression->constructor.type_if_matrix)
        {
            result_type_id = expression->constructor.type_if_matrix->spirv_id;
        }
        else if (expression->constructor.type_if_struct)
        {
            result_type_id = expression->constructor.type_if_struct->spirv_id_value;
        }
        else
        {
            // No type, something must be off with resolve.
            KAN_ASSERT (KAN_FALSE)
            result_type_id = (spirv_size_t) SPIRV_FIXED_ID_TYPE_VOID;
        }

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
            context, spirv_find_or_generate_variable_type (context, &variable->variable->type, 0u),
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
        case COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_REFERENCE:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS_INPUT:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS_OUTPUT:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_INTEGER_LITERAL:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_FLOATING_LITERAL:
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
        case COMPILER_INSTANCE_EXPRESSION_TYPE_SAMPLER_CALL:
        case COMPILER_INSTANCE_EXPRESSION_TYPE_CONSTRUCTOR:
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
    kan_bool_t reads_globals = function->first_buffer_access || function->first_sampler_access;

    struct compiler_instance_buffer_access_node_t *buffer_access = function->first_buffer_access;
    while (buffer_access)
    {
        switch (buffer_access->buffer->type)
        {
        case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
        case KAN_RPL_BUFFER_TYPE_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
            break;

        case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
            writes_globals |= function->required_stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX;
            break;

        case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
            writes_globals |= function->required_stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT;
            break;
        }

        buffer_access = buffer_access->next;
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
            context, spirv_find_or_generate_variable_type (context, &argument_variable->variable->type, 0u),
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

    struct compiler_instance_buffer_node_t *buffer = instance->first_buffer;
    while (buffer)
    {
        if (!buffer->used)
        {
            buffer = buffer->next;
            continue;
        }

        if (buffer->first_flattened_declaration)
        {
            buffer->structured_variable_spirv_id = UINT32_MAX;
            struct compiler_instance_buffer_flattened_declaration_t *declaration = buffer->first_flattened_declaration;

            while (declaration)
            {
                declaration->spirv_id_input = UINT32_MAX;
                declaration->spirv_id_output = UINT32_MAX;

                switch (buffer->type)
                {
                case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
                case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
                    spirv_emit_flattened_input_variable (&context, declaration, KAN_FALSE);
                    break;

                case KAN_RPL_BUFFER_TYPE_UNIFORM:
                case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
                    KAN_ASSERT (KAN_FALSE)
                    break;

                case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
                    spirv_emit_flattened_input_variable (&context, declaration, KAN_TRUE);
                    spirv_emit_flattened_output_variable (&context, declaration);
                    break;

                case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
                    spirv_emit_flattened_output_variable (&context, declaration);
                    break;
                }

                declaration = declaration->next;
            }
        }
        else
        {
            spirv_size_t buffer_struct_id = context.current_bound;
            ++context.current_bound;
            spirv_emit_struct_from_declaration_list (&context, buffer->first_field, buffer->name, buffer_struct_id);

            switch (buffer->type)
            {
            case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
            case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
            case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
                break;

            case KAN_RPL_BUFFER_TYPE_UNIFORM:
            case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
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

            spirv_emit_descriptor_set (&context, buffer->structured_variable_spirv_id, (spirv_size_t) buffer->set);
            spirv_emit_binding (&context, buffer->structured_variable_spirv_id, buffer->binding);
            spirv_generate_op_name (&context, buffer->structured_variable_spirv_id, buffer->name);
        }

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

        switch (sampler->type)
        {
        case KAN_RPL_SAMPLER_TYPE_2D:
            sampler_code[1u] = SPIRV_FIXED_ID_TYPE_SAMPLER_2D_POINTER;
            break;
        }

        sampler_code[2u] = sampler->variable_spirv_id;
        sampler_code[3u] = SpvStorageClassUniformConstant;

        spirv_emit_descriptor_set (&context, sampler->variable_spirv_id, (spirv_size_t) sampler->set);
        spirv_emit_binding (&context, sampler->variable_spirv_id, sampler->binding);
        spirv_generate_op_name (&context, sampler->variable_spirv_id, sampler->name);
        sampler = sampler->next;
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
