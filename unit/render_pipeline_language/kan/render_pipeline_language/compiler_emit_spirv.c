#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.h>

#define KAN_RPL_COMPILER_IMPLEMENTATION
#include <kan/render_pipeline_language/compiler_internal.h>

struct spirv_arbitrary_instruction_item_t
{
    struct spirv_arbitrary_instruction_item_t *next;
    uint32_t code[];
};

struct spirv_arbitrary_instruction_section_t
{
    struct spirv_arbitrary_instruction_item_t *first;
    struct spirv_arbitrary_instruction_item_t *last;
};

struct spirv_generation_array_type_t
{
    struct spirv_generation_array_type_t *next;
    uint32_t spirv_id;
    uint32_t spirv_function_pointer_id;
    struct inbuilt_vector_type_t *base_type_if_vector;
    struct inbuilt_matrix_type_t *base_type_if_matrix;
    struct compiler_instance_struct_node_t *base_type_if_struct;
    uint64_t dimensions_count;
    uint64_t *dimensions;
};

struct spirv_generation_function_type_t
{
    struct spirv_generation_function_type_t *next;
    uint64_t argument_count;
    uint32_t generated_id;
    uint32_t return_type_id;
    uint32_t *argument_types;
};

struct spirv_generation_block_t
{
    struct spirv_generation_block_t *next;
    uint32_t spirv_id;

    /// \details We need to store variables in the first function block right after the label.
    ///          Therefore label and variables have separate section.
    struct spirv_arbitrary_instruction_section_t header_section;

    struct spirv_arbitrary_instruction_section_t code_section;
};

struct spirv_generation_temporary_variable_t
{
    struct spirv_generation_temporary_variable_t *next;
    uint32_t spirv_id;
    uint32_t spirv_type_id;
};

struct spirv_generation_builtin_used_by_stage_t
{
    struct spirv_generation_builtin_used_by_stage_t *next;
    enum kan_rpl_pipeline_stage_t stage;
};

struct spirv_generation_builtin_t
{
    struct spirv_generation_builtin_t *next;
    uint32_t spirv_id;
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
    uint32_t spirv_id;
    int32_t value;
};

struct spirv_generation_floating_constant_t
{
    struct spirv_generation_floating_constant_t *next;
    uint32_t spirv_id;
    float value;
};

struct spirv_generation_context_t
{
    struct rpl_compiler_instance_t *instance;
    uint32_t current_bound;
    uint32_t code_word_count;
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
};

static inline uint32_t *spirv_new_instruction (struct spirv_generation_context_t *context,
                                               struct spirv_arbitrary_instruction_section_t *section,
                                               uint32_t word_count)
{
    struct spirv_arbitrary_instruction_item_t *item = kan_stack_group_allocator_allocate (
        &context->temporary_allocator,
        sizeof (struct spirv_arbitrary_instruction_item_t) + sizeof (uint32_t) * word_count,
        _Alignof (struct spirv_arbitrary_instruction_item_t));

    item->next = NULL;
    item->code[0u] = word_count << SpvWordCountShift;

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

    context->code_word_count += word_count;
    return item->code;
}

static inline uint32_t spirv_to_word_length (uint32_t length)
{
    return (length + 1u) % sizeof (uint32_t) == 0u ? (length + 1u) / sizeof (uint32_t) :
                                                     1u + (length + 1u) / sizeof (uint32_t);
}

static inline void spirv_generate_op_name (struct spirv_generation_context_t *context,
                                           uint32_t for_id,
                                           const char *name)
{
    const uint32_t length = (uint32_t) strlen (name);
    const uint32_t word_length = spirv_to_word_length (length);
    uint32_t *code = spirv_new_instruction (context, &context->debug_section, 2u + word_length);
    code[0u] |= SpvOpCodeMask & SpvOpName;
    code[1u] = for_id;
    code[1u + word_length] = 0u;
    memcpy ((uint8_t *) (code + 2u), name, length);
}

static inline void spirv_generate_op_member_name (struct spirv_generation_context_t *context,
                                                  uint32_t struct_id,
                                                  uint32_t member_index,
                                                  const char *name)
{
    const uint32_t length = (uint32_t) strlen (name);
    const uint32_t word_length = spirv_to_word_length (length);
    uint32_t *code = spirv_new_instruction (context, &context->debug_section, 3u + word_length);
    code[0u] |= SpvOpCodeMask & SpvOpMemberName;
    code[1u] = struct_id;
    code[2u] = member_index;
    code[2u + word_length] = 0u;
    memcpy ((uint8_t *) (code + 3u), name, length);
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

    uint32_t *code = spirv_new_instruction (context, &context->base_type_section, 2u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVoid;
    code[1u] = SPIRV_FIXED_ID_TYPE_VOID;

    code = spirv_new_instruction (context, &context->base_type_section, 2u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeBool;
    code[1u] = SPIRV_FIXED_ID_TYPE_BOOLEAN;

    code = spirv_new_instruction (context, &context->base_type_section, 3u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeFloat;
    code[1u] = SPIRV_FIXED_ID_TYPE_FLOAT;
    code[2u] = 32u;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_FLOAT_INPUT_POINTER;
    code[2u] = SpvStorageClassInput;
    code[3u] = SPIRV_FIXED_ID_TYPE_FLOAT;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_FLOAT_OUTPUT_POINTER;
    code[2u] = SpvStorageClassOutput;
    code[3u] = SPIRV_FIXED_ID_TYPE_FLOAT;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_FLOAT_FUNCTION_POINTER;
    code[2u] = SpvStorageClassFunction;
    code[3u] = SPIRV_FIXED_ID_TYPE_FLOAT;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeInt;
    code[1u] = SPIRV_FIXED_ID_TYPE_INTEGER;
    code[2u] = 32u;
    code[3u] = 1u; // Signed.

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_INTEGER_INPUT_POINTER;
    code[2u] = SpvStorageClassInput;
    code[3u] = SPIRV_FIXED_ID_TYPE_INTEGER;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_INTEGER_OUTPUT_POINTER;
    code[2u] = SpvStorageClassOutput;
    code[3u] = SPIRV_FIXED_ID_TYPE_INTEGER;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_INTEGER_FUNCTION_POINTER;
    code[2u] = SpvStorageClassFunction;
    code[3u] = SPIRV_FIXED_ID_TYPE_INTEGER;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_F2;
    code[2u] = SPIRV_FIXED_ID_TYPE_FLOAT;
    code[3u] = 2u;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_F2_INPUT_POINTER;
    code[2u] = SpvStorageClassInput;
    code[3u] = SPIRV_FIXED_ID_TYPE_F2;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_F2_OUTPUT_POINTER;
    code[2u] = SpvStorageClassOutput;
    code[3u] = SPIRV_FIXED_ID_TYPE_F2;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_F2_FUNCTION_POINTER;
    code[2u] = SpvStorageClassFunction;
    code[3u] = SPIRV_FIXED_ID_TYPE_F2;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_F3;
    code[2u] = SPIRV_FIXED_ID_TYPE_FLOAT;
    code[3u] = 3u;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_F3_INPUT_POINTER;
    code[2u] = SpvStorageClassInput;
    code[3u] = SPIRV_FIXED_ID_TYPE_F3;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_F3_OUTPUT_POINTER;
    code[2u] = SpvStorageClassOutput;
    code[3u] = SPIRV_FIXED_ID_TYPE_F3;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_F3_FUNCTION_POINTER;
    code[2u] = SpvStorageClassFunction;
    code[3u] = SPIRV_FIXED_ID_TYPE_F3;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_F4;
    code[2u] = SPIRV_FIXED_ID_TYPE_FLOAT;
    code[3u] = 4u;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_F4_INPUT_POINTER;
    code[2u] = SpvStorageClassInput;
    code[3u] = SPIRV_FIXED_ID_TYPE_F4;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_F4_OUTPUT_POINTER;
    code[2u] = SpvStorageClassOutput;
    code[3u] = SPIRV_FIXED_ID_TYPE_F4;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_F4_FUNCTION_POINTER;
    code[2u] = SpvStorageClassFunction;
    code[3u] = SPIRV_FIXED_ID_TYPE_F4;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_I2;
    code[2u] = SPIRV_FIXED_ID_TYPE_INTEGER;
    code[3u] = 2u;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_I2_INPUT_POINTER;
    code[2u] = SpvStorageClassInput;
    code[3u] = SPIRV_FIXED_ID_TYPE_I2;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_I2_OUTPUT_POINTER;
    code[2u] = SpvStorageClassOutput;
    code[3u] = SPIRV_FIXED_ID_TYPE_I2;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_I2_FUNCTION_POINTER;
    code[2u] = SpvStorageClassFunction;
    code[3u] = SPIRV_FIXED_ID_TYPE_I2;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_I3;
    code[2u] = SPIRV_FIXED_ID_TYPE_INTEGER;
    code[3u] = 3u;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_I3_INPUT_POINTER;
    code[2u] = SpvStorageClassInput;
    code[3u] = SPIRV_FIXED_ID_TYPE_I3;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_I3_OUTPUT_POINTER;
    code[2u] = SpvStorageClassOutput;
    code[3u] = SPIRV_FIXED_ID_TYPE_I3;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_I3_FUNCTION_POINTER;
    code[2u] = SpvStorageClassFunction;
    code[3u] = SPIRV_FIXED_ID_TYPE_I3;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_I4;
    code[2u] = SPIRV_FIXED_ID_TYPE_INTEGER;
    code[3u] = 4u;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_I4_INPUT_POINTER;
    code[2u] = SpvStorageClassInput;
    code[3u] = SPIRV_FIXED_ID_TYPE_I4;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_I4_OUTPUT_POINTER;
    code[2u] = SpvStorageClassOutput;
    code[3u] = SPIRV_FIXED_ID_TYPE_I4;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_I4_FUNCTION_POINTER;
    code[2u] = SpvStorageClassFunction;
    code[3u] = SPIRV_FIXED_ID_TYPE_I4;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeMatrix;
    code[1u] = SPIRV_FIXED_ID_TYPE_F3X3;
    code[2u] = SPIRV_FIXED_ID_TYPE_F3;
    code[3u] = 3u;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_F3X3_INPUT_POINTER;
    code[2u] = SpvStorageClassInput;
    code[3u] = SPIRV_FIXED_ID_TYPE_F3X3;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_F3X3_OUTPUT_POINTER;
    code[2u] = SpvStorageClassOutput;
    code[3u] = SPIRV_FIXED_ID_TYPE_F3X3;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_F3X3_FUNCTION_POINTER;
    code[2u] = SpvStorageClassFunction;
    code[3u] = SPIRV_FIXED_ID_TYPE_F3X3;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeMatrix;
    code[1u] = SPIRV_FIXED_ID_TYPE_F4X4;
    code[2u] = SPIRV_FIXED_ID_TYPE_F4;
    code[3u] = 4u;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_F4X4_INPUT_POINTER;
    code[2u] = SpvStorageClassInput;
    code[3u] = SPIRV_FIXED_ID_TYPE_F4X4;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_F4X4_OUTPUT_POINTER;
    code[2u] = SpvStorageClassOutput;
    code[3u] = SPIRV_FIXED_ID_TYPE_F4X4;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_F4X4_FUNCTION_POINTER;
    code[2u] = SpvStorageClassFunction;
    code[3u] = SPIRV_FIXED_ID_TYPE_F4X4;

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
    code[7u] = 1u;
    code[8u] = SpvImageFormatUnknown;

    code = spirv_new_instruction (context, &context->base_type_section, 3u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeSampledImage;
    code[1u] = SPIRV_FIXED_ID_TYPE_SAMPLER_2D;
    code[2u] = SPIRV_FIXED_ID_TYPE_SAMPLER_2D_IMAGE;

    code = spirv_new_instruction (context, &context->base_type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    code[1u] = SPIRV_FIXED_ID_TYPE_SAMPLER_2D_POINTER;
    code[2u] = SpvStorageClassUniformConstant;
    code[3u] = SPIRV_FIXED_ID_TYPE_SAMPLER_2D;
}

static void spirv_init_generation_context (struct spirv_generation_context_t *context,
                                           struct rpl_compiler_instance_t *instance)
{
    context->instance = instance;
    context->current_bound = (uint32_t) SPIRV_FIXED_ID_END;
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
    spirv_generate_standard_types (context);
}

static inline void spirv_copy_instructions (uint32_t **output,
                                            struct spirv_arbitrary_instruction_item_t *instruction_item)
{
    while (instruction_item)
    {
        const uint32_t word_count = (*instruction_item->code & ~SpvOpCodeMask) >> SpvWordCountShift;
        memcpy (*output, instruction_item->code, word_count * sizeof (uint32_t));
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
        uint32_t *op_shader_capability = spirv_new_instruction (context, &base_section, 2u);
        *op_shader_capability |= SpvOpCodeMask & SpvOpCapability;
        *(op_shader_capability + 1u) = SpvCapabilityShader;
        break;
    }
    }

    static const char glsl_library_padded[] = "GLSL.std.450\0\0\0";
    _Static_assert (sizeof (glsl_library_padded) % sizeof (uint32_t) == 0u, "GLSL library name is really padded.");
    uint32_t *op_glsl_import =
        spirv_new_instruction (context, &base_section, 2u + sizeof (glsl_library_padded) / sizeof (uint32_t));
    op_glsl_import[0u] |= SpvOpCodeMask & SpvOpExtInstImport;
    op_glsl_import[1u] = (uint32_t) SPIRV_FIXED_ID_GLSL_LIBRARY;
    memcpy (&op_glsl_import[2u], glsl_library_padded, sizeof (glsl_library_padded));

    switch (context->instance->pipeline_type)
    {
    case KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC:
    {
        uint32_t *op_memory_model = spirv_new_instruction (context, &base_section, 3u);
        op_memory_model[0u] |= SpvOpCodeMask & SpvOpMemoryModel;
        op_memory_model[1u] = SpvAddressingModelLogical;
        op_memory_model[2u] = SpvMemoryModelGLSL450;
        break;
    }
    }

    // TODO: Generate entry points.

    kan_dynamic_array_set_capacity (code_output, 5u + context->code_word_count);
    code_output->size = code_output->capacity;

    uint32_t *output = (uint32_t *) code_output->data;
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

static uint32_t spirv_request_i1_constant (struct spirv_generation_context_t *context, int32_t value)
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

    uint32_t constant_id = context->current_bound;
    ++context->current_bound;

    uint32_t *constant_code = spirv_new_instruction (context, &context->base_type_section, 4u);
    constant_code[0u] |= SpvOpCodeMask & SpvOpConstant;
    constant_code[1u] = STATICS.type_i1.spirv_id;
    constant_code[2u] = constant_id;
    *(int32_t *) &constant_code[3u] = (int32_t) value;

    struct spirv_generation_integer_constant_t *new_constant = kan_stack_group_allocator_allocate (
        &context->temporary_allocator, sizeof (struct spirv_generation_integer_constant_t),
        _Alignof (struct spirv_generation_integer_constant_t));

    new_constant->next = context->first_integer_constant;
    context->first_integer_constant = new_constant;
    new_constant->spirv_id = constant_id;
    new_constant->value = value;
    return constant_id;
}

static uint32_t spirv_request_f1_constant (struct spirv_generation_context_t *context, float value)
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

    uint32_t constant_id = context->current_bound;
    ++context->current_bound;

    uint32_t *constant_code = spirv_new_instruction (context, &context->base_type_section, 4u);
    constant_code[0u] |= SpvOpCodeMask & SpvOpConstant;
    constant_code[1u] = STATICS.type_f1.spirv_id;
    constant_code[2u] = constant_id;
    *(float *) &constant_code[3u] = value;

    struct spirv_generation_floating_constant_t *new_constant = kan_stack_group_allocator_allocate (
        &context->temporary_allocator, sizeof (struct spirv_generation_floating_constant_t),
        _Alignof (struct spirv_generation_floating_constant_t));

    new_constant->next = context->first_floating_constant;
    context->first_floating_constant = new_constant;
    new_constant->spirv_id = constant_id;
    new_constant->value = value;
    return constant_id;
}

static uint32_t spirv_find_or_generate_variable_type (struct spirv_generation_context_t *context,
                                                      struct compiler_instance_full_type_definition_t *type,
                                                      uint64_t start_dimension_index,
                                                      kan_bool_t need_function_pointer_type)
{
    if (start_dimension_index == type->array_dimensions_count)
    {
        if (type->if_vector)
        {
            return need_function_pointer_type ? type->if_vector->spirv_id_function_pointer : type->if_vector->spirv_id;
        }
        else if (type->if_matrix)
        {
            return need_function_pointer_type ? type->if_matrix->spirv_id_function_pointer : type->if_matrix->spirv_id;
        }
        else if (type->if_struct)
        {
            return need_function_pointer_type ? type->if_struct->spirv_id_function_pointer :
                                                type->if_struct->spirv_id_value;
        }

        KAN_ASSERT (KAN_FALSE)
    }

    struct spirv_generation_array_type_t *array_type = context->first_generated_array_type;
    while (array_type)
    {
        if (array_type->base_type_if_vector == type->if_vector && array_type->base_type_if_matrix == type->if_matrix &&
            array_type->base_type_if_struct == type->if_struct &&
            array_type->dimensions_count == type->array_dimensions_count - start_dimension_index &&
            memcmp (array_type->dimensions, &type->array_dimensions[start_dimension_index],
                    array_type->dimensions_count * sizeof (uint64_t)) == 0)
        {
            return array_type->spirv_id;
        }

        array_type = array_type->next;
    }

    const uint32_t base_type_id =
        spirv_find_or_generate_variable_type (context, type, start_dimension_index + 1u, KAN_FALSE);
    const uint32_t constant_id =
        spirv_request_i1_constant (context, (uint32_t) type->array_dimensions[start_dimension_index]);

    uint32_t array_type_id = context->current_bound;
    ++context->current_bound;

    uint32_t *dimension_type_code = spirv_new_instruction (context, &context->base_type_section, 4u);
    dimension_type_code[0u] |= SpvOpCodeMask & SpvOpTypeArray;
    dimension_type_code[1u] = array_type_id;
    dimension_type_code[2u] = base_type_id;
    dimension_type_code[3u] = constant_id;

    uint32_t array_type_function_pointer_id = context->current_bound;
    ++context->current_bound;

    uint32_t *function_pointer_code = spirv_new_instruction (context, &context->base_type_section, 4u);
    function_pointer_code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
    function_pointer_code[1u] = array_type_function_pointer_id;
    function_pointer_code[2u] = SpvStorageClassFunction;
    function_pointer_code[3u] = array_type_id;

    struct spirv_generation_array_type_t *new_array_type = kan_stack_group_allocator_allocate (
        &context->temporary_allocator, sizeof (struct spirv_generation_array_type_t),
        _Alignof (struct spirv_generation_array_type_t));

    new_array_type->next = context->first_generated_array_type;
    context->first_generated_array_type = new_array_type;
    new_array_type->spirv_id = array_type_id;
    new_array_type->spirv_function_pointer_id = array_type_function_pointer_id;
    new_array_type->base_type_if_vector = type->if_vector;
    new_array_type->base_type_if_matrix = type->if_matrix;
    new_array_type->base_type_if_struct = type->if_struct;
    new_array_type->dimensions_count = type->array_dimensions_count - start_dimension_index;
    new_array_type->dimensions = &type->array_dimensions[start_dimension_index];

    return need_function_pointer_type ? new_array_type->spirv_function_pointer_id : new_array_type->spirv_id;
}

static inline void spirv_emit_struct_from_declaration_list (struct spirv_generation_context_t *context,
                                                            struct compiler_instance_declaration_node_t *first_field,
                                                            const char *debug_struct_name,
                                                            uint32_t struct_id)
{
    spirv_generate_op_name (context, struct_id, debug_struct_name);
    uint64_t field_count = 0u;
    struct compiler_instance_declaration_node_t *field = first_field;

    while (field)
    {
        ++field_count;
        field = field->next;
    }

    uint32_t *struct_code = spirv_new_instruction (context, &context->higher_type_section, 2u + field_count);
    struct_code[0u] |= SpvOpCodeMask & SpvOpTypeStruct;
    struct_code[1u] = struct_id;

    uint64_t field_index = 0u;
    field = first_field;

    while (field)
    {
        uint32_t field_type_id = spirv_find_or_generate_variable_type (context, &field->variable.type, 0u, KAN_FALSE);
        struct_code[2u + field_index] = field_type_id;
        spirv_generate_op_member_name (context, struct_id, (uint32_t) field_index, field->variable.name);
        field = field->next;
        ++field_index;
    }
}

static inline void spirv_emit_location (struct spirv_generation_context_t *context, uint32_t for_id, uint32_t location)
{
    uint32_t *location_code = spirv_new_instruction (context, &context->decoration_section, 4u);
    location_code[0u] |= SpvOpCodeMask & SpvOpDecorate;
    location_code[1u] = for_id;
    location_code[2u] = SpvDecorationLocation;
    location_code[3u] = location;
}

static inline void spirv_emit_binding (struct spirv_generation_context_t *context, uint32_t for_id, uint32_t binding)
{
    uint32_t *binding_code = spirv_new_instruction (context, &context->decoration_section, 4u);
    binding_code[0u] |= SpvOpCodeMask & SpvOpDecorate;
    binding_code[1u] = for_id;
    binding_code[2u] = SpvDecorationBinding;
    binding_code[3u] = binding;
}

static inline void spirv_emit_flattened_input_variable (
    struct spirv_generation_context_t *context, struct compiler_instance_buffer_flattened_declaration_t *declaration)
{
    declaration->spirv_id_input = context->current_bound;
    ++context->current_bound;

    uint32_t *variable_code = spirv_new_instruction (context, &context->global_variable_section, 4u);
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
}

static inline void spirv_emit_flattened_output_variable (
    struct spirv_generation_context_t *context, struct compiler_instance_buffer_flattened_declaration_t *declaration)
{
    declaration->spirv_id_output = context->current_bound;
    ++context->current_bound;

    uint32_t *variable_code = spirv_new_instruction (context, &context->global_variable_section, 4u);
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
    uint32_t return_type;
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

    uint64_t argument_count = 0u;
    struct compiler_instance_declaration_node_t *argument = function->first_argument;

    while (argument)
    {
        ++argument_count;
        argument = argument->next;
    }

    uint32_t *argument_types = NULL;
    if (argument_count > 0u)
    {
        argument_types = kan_stack_group_allocator_allocate (&context->temporary_allocator,
                                                             sizeof (uint32_t) * argument_count, _Alignof (uint32_t));
        uint64_t argument_index = 0u;
        argument = function->first_argument;

        while (argument)
        {
            argument_types[argument_index] =
                spirv_find_or_generate_variable_type (context, &argument->variable.type, 0u, KAN_TRUE);
            ++argument_index;
            argument = argument->next;
        }
    }

    struct spirv_generation_function_type_t *function_type = context->first_generated_function_type;
    while (function_type)
    {
        if (function_type->return_type_id == return_type && function_type->argument_count == argument_count &&
            (argument_count == 0u ||
             memcmp (function_type->argument_types, argument_types, argument_count * sizeof (uint32_t)) == 0))
        {
            return function_type;
        }

        function_type = function_type->next;
    }

    uint32_t function_type_id = context->current_bound;
    ++context->current_bound;

    uint32_t *type_code = spirv_new_instruction (context, &context->higher_type_section, 3u + argument_count);
    type_code[0u] |= SpvOpCodeMask & SpvOpTypeFunction;
    type_code[1u] = function_type_id;
    type_code[2u] = return_type;

    if (argument_count > 0u)
    {
        memcpy (type_code + 3u, argument_types, argument_count * sizeof (uint32_t));
    }

    struct spirv_generation_function_type_t *new_function_type = kan_stack_group_allocator_allocate (
        &context->temporary_allocator, sizeof (struct spirv_generation_function_type_t),
        _Alignof (struct spirv_generation_function_type_t));

    new_function_type->next = context->first_generated_function_type;
    context->first_generated_function_type = new_function_type;

    new_function_type->generated_id = function_type_id;
    new_function_type->return_type_id = return_type;
    new_function_type->argument_count = argument_count;
    new_function_type->argument_types = argument_types;
    return new_function_type;
}

static inline struct spirv_generation_block_t *spirv_function_new_block (struct spirv_generation_context_t *context,
                                                                         struct spirv_generation_function_node_t *node)
{
    struct spirv_generation_block_t *block =
        kan_stack_group_allocator_allocate (&context->temporary_allocator, sizeof (struct spirv_generation_block_t),
                                            _Alignof (struct spirv_generation_block_t));

    block->next = NULL;
    block->spirv_id = context->current_bound;
    ++context->current_bound;
    block->header_section.first = NULL;
    block->header_section.last = NULL;
    block->code_section.first = NULL;
    block->code_section.last = NULL;

    uint32_t *label_code = spirv_new_instruction (context, &block->header_section, 2u);
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

// We should never try to load something other than single vectors or matrices.
// If we do it, then something is wrong with resolve or AST.
#define SPIRV_ASSERT_VARIABLE_CAN_BE_LOADED(VARIABLE)                                                                  \
    KAN_ASSERT ((VARIABLE)->type.array_dimensions_count == 0u &&                                                       \
                ((VARIABLE)->type.if_vector || (VARIABLE)->type.if_matrix))

#define SPIRV_LOADED_VARIABLE_TYPE(VARIABLE)                                                                           \
    ((VARIABLE)->type.if_vector ? (VARIABLE)->type.if_vector->spirv_id : (VARIABLE)->type.if_matrix->spirv_id)

static inline uint32_t spirv_emit_load (struct spirv_generation_context_t *context,
                                        struct spirv_arbitrary_instruction_section_t *section,
                                        uint32_t type_id,
                                        uint32_t variable_id)
{
    uint32_t loaded_id = context->current_bound;
    ++context->current_bound;

    uint32_t *load_code = spirv_new_instruction (context, section, 4u);
    load_code[0u] |= SpvOpCodeMask & SpvOpLoad;
    load_code[1u] = type_id;
    load_code[2u] = loaded_id;
    load_code[3u] = variable_id;
    return loaded_id;
}

static inline void spirv_emit_store (struct spirv_generation_context_t *context,
                                     struct spirv_arbitrary_instruction_section_t *section,
                                     uint32_t variable_id,
                                     uint32_t data_id)
{
    uint32_t *load_code = spirv_new_instruction (context, section, 3u);
    load_code[0u] |= SpvOpCodeMask & SpvOpStore;
    load_code[1u] = variable_id;
    load_code[2u] = data_id;
}

#define SPIRV_EMIT_VECTOR_ARITHMETIC(SUFFIX, FLOAT_OP, INTEGER_OP)                                                     \
    static inline uint32_t spirv_emit_vector_##SUFFIX (                                                                \
        struct spirv_generation_context_t *context, struct spirv_arbitrary_instruction_section_t *section,             \
        struct inbuilt_vector_type_t *type, uint32_t left, uint32_t right)                                             \
    {                                                                                                                  \
        uint32_t result_id = context->current_bound;                                                                   \
        ++context->current_bound;                                                                                      \
                                                                                                                       \
        switch (type->item)                                                                                            \
        {                                                                                                              \
        case INBUILT_TYPE_ITEM_FLOAT:                                                                                  \
        {                                                                                                              \
            uint32_t *code = spirv_new_instruction (context, section, 5u);                                             \
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
            uint32_t *code = spirv_new_instruction (context, section, 5u);                                             \
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
    static inline uint32_t spirv_emit_matrix_##SUFFIX (                                                                \
        struct spirv_generation_context_t *context, struct spirv_arbitrary_instruction_section_t *section,             \
        struct inbuilt_matrix_type_t *type, uint32_t left, uint32_t right)                                             \
    {                                                                                                                  \
        uint32_t column_result_ids[4u];                                                                                \
        KAN_ASSERT (type->columns <= 4u)                                                                               \
        struct inbuilt_vector_type_t *column_type;                                                                     \
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
        for (uint64_t column_index = 0u; column_index < type->columns; ++column_index)                                 \
        {                                                                                                              \
            uint32_t left_extract_result = context->current_bound;                                                     \
            ++context->current_bound;                                                                                  \
                                                                                                                       \
            uint32_t *left_extract = spirv_new_instruction (context, section, 5u);                                     \
            left_extract[0u] |= SpvOpCodeMask & SpvOpCompositeExtract;                                                 \
            left_extract[1u] = column_type->spirv_id;                                                                  \
            left_extract[2u] = left_extract_result;                                                                    \
            left_extract[3u] = left;                                                                                   \
            left_extract[4u] = (uint32_t) column_index;                                                                \
                                                                                                                       \
            uint32_t right_extract_result = context->current_bound;                                                    \
            ++context->current_bound;                                                                                  \
                                                                                                                       \
            uint32_t *right_extract = spirv_new_instruction (context, section, 5u);                                    \
            right_extract[0u] |= SpvOpCodeMask & SpvOpCompositeExtract;                                                \
            right_extract[1u] = column_type->spirv_id;                                                                 \
            right_extract[2u] = right_extract_result;                                                                  \
            right_extract[3u] = right;                                                                                 \
            right_extract[4u] = (uint32_t) column_index;                                                               \
                                                                                                                       \
            column_result_ids[column_index] =                                                                          \
                spirv_emit_vector_##SUFFIX (context, section, column_type, left_extract_result, right_extract_result); \
        }                                                                                                              \
                                                                                                                       \
        uint32_t result_id = context->current_bound;                                                                   \
        ++context->current_bound;                                                                                      \
                                                                                                                       \
        uint32_t *construct = spirv_new_instruction (context, section, 3u + type->columns);                            \
        construct[0u] |= SpvOpCodeMask & SpvOpCompositeConstruct;                                                      \
        construct[1u] |= type->spirv_id;                                                                               \
        construct[2u] |= result_id;                                                                                    \
        memcpy (construct + 3u, column_result_ids, type->columns * sizeof (uint32_t));                                 \
        return result_id;                                                                                              \
    }

SPIRV_EMIT_MATRIX_ARITHMETIC (add)
SPIRV_EMIT_MATRIX_ARITHMETIC (sub)
SPIRV_EMIT_MATRIX_ARITHMETIC (div)
#undef SPIRV_EMIT_MATRIX_ARITHMETIC

static uint32_t spirv_use_temporary_variable (struct spirv_generation_context_t *context,
                                              struct spirv_generation_function_node_t *function,
                                              struct compiler_instance_expression_output_type_t *required_type)
{
    // We do not expect temporary variables to be arrays or booleans.
    // If they are, then something is wrong with the resolve.
    KAN_ASSERT (required_type->type.array_dimensions_count == 0u)
    KAN_ASSERT (!required_type->boolean)

    uint32_t required_type_id;
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
        required_type_id = (uint32_t) SPIRV_FIXED_ID_INVALID;
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

    struct spirv_generation_temporary_variable_t *new_variable = kan_stack_group_allocator_allocate (
        &context->temporary_allocator, sizeof (struct spirv_generation_temporary_variable_t),
        _Alignof (struct spirv_generation_temporary_variable_t));

    new_variable->next = function->first_used_temporary_variable;
    function->first_used_temporary_variable = new_variable;
    new_variable->spirv_id = context->current_bound;
    ++context->current_bound;
    new_variable->spirv_type_id = required_type_id;

    uint32_t *variable_code = spirv_new_instruction (context, &function->first_block->header_section, 4u);
    variable_code[0u] |= SpvOpCodeMask & SpvOpVariable;
    variable_code[1u] = required_type_id;
    variable_code[2u] = new_variable->spirv_id;
    variable_code[3u] = SpvStorageClassFunction;

    return new_variable->spirv_id;
}

static void spirv_if_temporary_variable_then_stop_using (struct spirv_generation_function_node_t *function,
                                                         uint32_t spirv_id)
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

static uint32_t spirv_emit_expression (struct spirv_generation_context_t *context,
                                       struct spirv_generation_function_node_t *function,
                                       struct spirv_generation_block_t *current_block,
                                       struct compiler_instance_expression_node_t *expression,
                                       kan_bool_t result_should_be_pointer);

static uint32_t spirv_emit_extension_instruction (struct spirv_generation_context_t *context,
                                                  struct spirv_generation_function_node_t *function,
                                                  struct spirv_generation_block_t *current_block,
                                                  struct compiler_instance_expression_node_t *expression,
                                                  uint32_t library,
                                                  uint32_t extension)
{
    uint64_t argument_count = 0u;
    struct compiler_instance_expression_list_item_t *argument = expression->function_call.first_argument;

    while (argument)
    {
        ++argument_count;
        argument = argument->next;
    }

    uint32_t result_id = context->current_bound;
    ++context->current_bound;

    uint32_t *arguments = NULL;
    if (argument_count > 0u)
    {
        arguments = kan_stack_group_allocator_allocate (&context->temporary_allocator,
                                                        sizeof (uint32_t) * argument_count, _Alignof (uint32_t));
        uint64_t argument_index = 0u;
        argument = expression->function_call.first_argument;

        while (argument)
        {
            arguments[argument_index] =
                spirv_emit_expression (context, function, current_block, argument->expression, KAN_TRUE);
            ++argument_index;
            argument = argument->next;
        }
    }

    uint32_t result_type_id;
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
        result_type_id = (uint32_t) SPIRV_FIXED_ID_TYPE_VOID;
    }

    uint32_t *code = spirv_new_instruction (context, &current_block->code_section, 5u + argument_count);
    code[0u] |= SpvOpCodeMask & SpvOpExtInst;
    code[1u] = result_type_id;
    code[2u] = result_id;
    code[3u] = library;
    code[4u] = extension;

    if (arguments)
    {
        memcpy (code + 5u, arguments, sizeof (uint32_t) * argument_count);
    }

    return result_id;
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

    stage_usage = kan_stack_group_allocator_allocate (&context->temporary_allocator,
                                                      sizeof (struct spirv_generation_builtin_used_by_stage_t),
                                                      _Alignof (struct spirv_generation_builtin_used_by_stage_t));
    stage_usage->next = builtin->first_stage;
    builtin->first_stage = stage_usage;
    stage_usage->stage = function->source->required_stage;
}

static uint32_t spirv_request_builtin (struct spirv_generation_context_t *context,
                                       struct spirv_generation_function_node_t *associated_function,
                                       SpvBuiltIn builtin_type,
                                       SpvStorageClass builtin_storage,
                                       uint32_t builtin_variable_type)
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

    uint32_t variable_id = context->current_bound;
    ++context->current_bound;

    struct spirv_generation_builtin_t *new_builtin =
        kan_stack_group_allocator_allocate (&context->temporary_allocator, sizeof (struct spirv_generation_builtin_t),
                                            _Alignof (struct spirv_generation_builtin_t));
    new_builtin->next = context->first_builtin;
    context->first_builtin = new_builtin;
    new_builtin->spirv_id = variable_id;
    new_builtin->builtin_type = builtin_type;
    new_builtin->builtin_storage = builtin_storage;
    new_builtin->first_stage = NULL;

    uint32_t *decoration_code = spirv_new_instruction (context, &context->decoration_section, 4u);
    decoration_code[0u] |= SpvOpCodeMask & SpvOpDecorate;
    decoration_code[1u] = variable_id;
    decoration_code[2u] = SpvDecorationBuiltIn;
    decoration_code[3u] = builtin_type;

    uint32_t *variable_code = spirv_new_instruction (context, &context->global_variable_section, 4u);
    variable_code[0u] |= SpvOpCodeMask & SpvOpVariable;
    variable_code[1u] = builtin_variable_type;
    variable_code[2u] = variable_id;
    variable_code[3u] = builtin_storage;

    spirv_register_builtin_usage (context, associated_function, new_builtin);
    return variable_id;
}

static inline uint32_t spirv_emit_inbuilt_function_call (struct spirv_generation_context_t *context,
                                                         struct spirv_generation_function_node_t *function,
                                                         struct spirv_generation_block_t *current_block,
                                                         struct compiler_instance_expression_node_t *expression)
{
    if (expression->function_call.function == &STATICS.glsl_450_sqrt)
    {
        return spirv_emit_extension_instruction (context, function, current_block, expression,
                                                 (uint32_t) SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Sqrt);
    }
    else if (expression->function_call.function == &STATICS.shader_standard_vertex_stage_output_position)
    {
        uint32_t operand_id = spirv_emit_expression (context, function, current_block,
                                                     expression->function_call.first_argument->expression, KAN_FALSE);

        const uint32_t position_builtin = spirv_request_builtin (
            context, function, SpvBuiltInPosition, SpvStorageClassOutput, STATICS.type_f4.spirv_id_output_pointer);

        spirv_emit_store (context, &current_block->code_section, position_builtin, operand_id);
        // Just a store operation, has no return.
        return (uint32_t) SPIRV_FIXED_ID_INVALID;
    }
    else if (expression->function_call.function == &STATICS.shader_standard_i1_to_f1 ||
             expression->function_call.function == &STATICS.shader_standard_i2_to_f2 ||
             expression->function_call.function == &STATICS.shader_standard_i3_to_f3 ||
             expression->function_call.function == &STATICS.shader_standard_i4_to_f4)
    {
        uint32_t operand_id = spirv_emit_expression (context, function, current_block,
                                                     expression->function_call.first_argument->expression, KAN_FALSE);

        uint32_t result_id = context->current_bound;
        ++context->current_bound;

        uint32_t *code = spirv_new_instruction (context, &current_block->code_section, 4u);
        code[0u] = SpvOpCodeMask & SpvOpConvertSToF;
        code[1u] = expression->function_call.function->return_type_if_vector->spirv_id;
        code[2u] = result_id;
        code[3u] = operand_id;

        return result_id;
    }

    // Unknown inbuilt function, how this happened?
    KAN_ASSERT (KAN_FALSE)
    return (uint32_t) SPIRV_FIXED_ID_INVALID;
}

static uint32_t spirv_emit_expression (struct spirv_generation_context_t *context,
                                       struct spirv_generation_function_node_t *function,
                                       struct spirv_generation_block_t *current_block,
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

        SPIRV_ASSERT_VARIABLE_CAN_BE_LOADED (expression->variable_reference->variable)
        return spirv_emit_load (context, &current_block->code_section,
                                SPIRV_LOADED_VARIABLE_TYPE (expression->variable_reference->variable),
                                expression->variable_reference->spirv_id);

    case COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS:
    {
        // TODO: Implement. Combine with array indexing?
        return (uint32_t) SPIRV_FIXED_ID_INVALID;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS_INPUT:
        if (result_should_be_pointer)
        {
            return expression->flattened_buffer_access->spirv_id_input;
        }

        SPIRV_ASSERT_VARIABLE_CAN_BE_LOADED (&expression->flattened_buffer_access->source_declaration->variable)
        return spirv_emit_load (
            context, &current_block->code_section,
            SPIRV_LOADED_VARIABLE_TYPE (&expression->flattened_buffer_access->source_declaration->variable),
            expression->flattened_buffer_access->spirv_id_input);

    case COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS_OUTPUT:
        if (result_should_be_pointer)
        {
            return expression->flattened_buffer_access->spirv_id_output;
        }

        SPIRV_ASSERT_VARIABLE_CAN_BE_LOADED (&expression->flattened_buffer_access->source_declaration->variable)
        return spirv_emit_load (
            context, &current_block->code_section,
            SPIRV_LOADED_VARIABLE_TYPE (&expression->flattened_buffer_access->source_declaration->variable),
            expression->flattened_buffer_access->spirv_id_output);

#define WRAP_OPERATION_RESULT_IF_NEEDED                                                                                \
    if (result_should_be_pointer)                                                                                      \
    {                                                                                                                  \
        uint32_t variable_id = spirv_use_temporary_variable (context, function, &expression->output);                  \
        spirv_emit_store (context, &current_block->code_section, variable_id, result_id);                              \
        result_id = variable_id;                                                                                       \
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_INTEGER_LITERAL:
    {
        uint32_t result_id = spirv_request_i1_constant (context, (int32_t) expression->integer_literal);
        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_FLOATING_LITERAL:
    {
        uint32_t result_id = spirv_request_f1_constant (context, (float) expression->floating_literal);
        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_DECLARATION:
        // If variable declaration result is requested as not pointer, then something is off with resolve or AST.
        KAN_ASSERT (result_should_be_pointer)
        return expression->variable_declaration.declared_in_scope->spirv_id;

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ARRAY_INDEX:
    {
        // TODO: Implement. Combine multiple array index operations into one access chain?
        return (uint32_t) SPIRV_FIXED_ID_INVALID;
    }

#define BINARY_OPERATION_COMMON_PREPARE                                                                                \
    const uint32_t left_operand_id = spirv_emit_expression (context, function, current_block,                          \
                                                            expression->binary_operation.left_operand, KAN_FALSE);     \
    const uint32_t right_operand_id = spirv_emit_expression (context, function, current_block,                         \
                                                             expression->binary_operation.right_operand, KAN_FALSE)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ADD:
    {
        BINARY_OPERATION_COMMON_PREPARE;
        uint32_t result_id;

        if (expression->output.type.if_vector)
        {
            result_id = spirv_emit_vector_add (context, &current_block->code_section, expression->output.type.if_vector,
                                               left_operand_id, right_operand_id);
        }
        else if (expression->output.type.if_matrix)
        {
            result_id = spirv_emit_matrix_add (context, &current_block->code_section, expression->output.type.if_matrix,
                                               left_operand_id, right_operand_id);
        }
        else
        {
            KAN_ASSERT (KAN_FALSE)
            result_id = (uint32_t) SPIRV_FIXED_ID_INVALID;
        }

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_SUBTRACT:
    {
        BINARY_OPERATION_COMMON_PREPARE;
        uint32_t result_id;

        if (expression->output.type.if_vector)
        {
            result_id = spirv_emit_vector_sub (context, &current_block->code_section, expression->output.type.if_vector,
                                               left_operand_id, right_operand_id);
        }
        else if (expression->output.type.if_matrix)
        {
            result_id = spirv_emit_matrix_sub (context, &current_block->code_section, expression->output.type.if_matrix,
                                               left_operand_id, right_operand_id);
        }
        else
        {
            KAN_ASSERT (KAN_FALSE)
            result_id = (uint32_t) SPIRV_FIXED_ID_INVALID;
        }

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_MULTIPLY:
    {
        BINARY_OPERATION_COMMON_PREPARE;
        uint32_t result_id;

        if (expression->binary_operation.left_operand->output.type.if_vector &&
            expression->binary_operation.left_operand->output.type.if_vector ==
                expression->binary_operation.right_operand->output.type.if_vector)
        {
            result_id = spirv_emit_vector_mul (context, &current_block->code_section, expression->output.type.if_vector,
                                               left_operand_id, right_operand_id);
        }
        else if (expression->binary_operation.left_operand->output.type.if_vector &&
                 expression->binary_operation.right_operand->output.type.if_vector &&
                 expression->binary_operation.right_operand->output.type.if_vector->items_count == 1u)
        {
            result_id = context->current_bound;
            ++context->current_bound;

            uint32_t *multiply = spirv_new_instruction (context, &current_block->code_section, 5u);
            multiply[0u] |= SpvOpCodeMask & SpvOpVectorTimesScalar;
            multiply[1u] |= expression->output.type.if_vector->spirv_id;
            multiply[2u] |= result_id;
            multiply[3u] |= left_operand_id;
            multiply[4u] |= right_operand_id;
        }
        else if (expression->binary_operation.left_operand->output.type.if_matrix &&
                 expression->binary_operation.right_operand->output.type.if_vector)
        {
            result_id = context->current_bound;
            ++context->current_bound;

            if (expression->binary_operation.right_operand->output.type.if_vector->items_count == 1u)
            {
                uint32_t *multiply = spirv_new_instruction (context, &current_block->code_section, 5u);
                multiply[0u] |= SpvOpCodeMask & SpvOpMatrixTimesScalar;
                multiply[1u] |= expression->output.type.if_matrix->spirv_id;
                multiply[2u] |= result_id;
                multiply[3u] |= left_operand_id;
                multiply[4u] |= right_operand_id;
            }
            else
            {
                uint32_t *multiply = spirv_new_instruction (context, &current_block->code_section, 5u);
                multiply[0u] |= SpvOpCodeMask & SpvOpMatrixTimesVector;
                multiply[1u] |= expression->output.type.if_vector->spirv_id;
                multiply[2u] |= result_id;
                multiply[3u] |= left_operand_id;
                multiply[4u] |= right_operand_id;
            }
        }
        else if (expression->binary_operation.left_operand->output.type.if_vector &&
                 expression->binary_operation.right_operand->output.type.if_matrix)
        {
            result_id = context->current_bound;
            ++context->current_bound;

            uint32_t *multiply = spirv_new_instruction (context, &current_block->code_section, 5u);
            multiply[0u] |= SpvOpCodeMask & SpvOpVectorTimesMatrix;
            multiply[1u] |= expression->output.type.if_vector->spirv_id;
            multiply[2u] |= result_id;
            multiply[3u] |= left_operand_id;
            multiply[4u] |= right_operand_id;
        }
        else if (expression->binary_operation.left_operand->output.type.if_matrix &&
                 expression->binary_operation.right_operand->output.type.if_matrix)
        {
            result_id = context->current_bound;
            ++context->current_bound;

            uint32_t *multiply = spirv_new_instruction (context, &current_block->code_section, 5u);
            multiply[0u] |= SpvOpCodeMask & SpvOpMatrixTimesMatrix;
            multiply[1u] |= expression->output.type.if_matrix->spirv_id;
            multiply[2u] |= result_id;
            multiply[3u] |= left_operand_id;
            multiply[4u] |= right_operand_id;
        }
        else
        {
            KAN_ASSERT (KAN_FALSE)
            result_id = (uint32_t) SPIRV_FIXED_ID_INVALID;
        }

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_DIVIDE:
    {
        BINARY_OPERATION_COMMON_PREPARE;
        uint32_t result_id;

        if (expression->binary_operation.left_operand->output.type.if_vector &&
            expression->binary_operation.left_operand->output.type.if_vector ==
                expression->binary_operation.right_operand->output.type.if_vector)
        {
            result_id = spirv_emit_vector_div (context, &current_block->code_section, expression->output.type.if_vector,
                                               left_operand_id, right_operand_id);
        }
        else if (expression->binary_operation.left_operand->output.type.if_matrix &&
                 expression->binary_operation.left_operand->output.type.if_matrix ==
                     expression->binary_operation.right_operand->output.type.if_matrix)
        {
            result_id = spirv_emit_matrix_div (context, &current_block->code_section, expression->output.type.if_matrix,
                                               left_operand_id, right_operand_id);
        }
        else if (expression->binary_operation.left_operand->output.type.if_vector &&
                 expression->binary_operation.right_operand->output.type.if_vector &&
                 expression->binary_operation.right_operand->output.type.if_vector->items_count == 1u)
        {
            uint32_t composite_id = context->current_bound;
            ++context->current_bound;

            uint32_t *construct = spirv_new_instruction (
                context, &current_block->code_section,
                3u + expression->binary_operation.left_operand->output.type.if_vector->items_count);
            construct[0u] |= SpvOpCodeMask & SpvOpCompositeConstruct;
            construct[1u] |= expression->binary_operation.left_operand->output.type.if_vector->spirv_id;
            construct[2u] |= composite_id;

            for (uint64_t index = 0u;
                 index < expression->binary_operation.left_operand->output.type.if_vector->items_count; ++index)
            {
                construct[3u + index] = right_operand_id;
            }

            result_id = spirv_emit_vector_div (context, &current_block->code_section,
                                               expression->binary_operation.left_operand->output.type.if_vector,
                                               left_operand_id, composite_id);
        }
        else
        {
            KAN_ASSERT (KAN_FALSE)
            result_id = (uint32_t) SPIRV_FIXED_ID_INVALID;
        }

        WRAP_OPERATION_RESULT_IF_NEEDED
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_MODULUS:
    {
        BINARY_OPERATION_COMMON_PREPARE;
        uint32_t result_id = context->current_bound;
        ++context->current_bound;

        uint32_t *code = spirv_new_instruction (context, &current_block->code_section, 5u);
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
        const uint32_t left_operand_id = spirv_emit_expression (context, function, current_block,
                                                                expression->binary_operation.left_operand, KAN_TRUE);
        const uint32_t right_operand_id = spirv_emit_expression (context, function, current_block,
                                                                 expression->binary_operation.right_operand, KAN_FALSE);

        spirv_emit_store (context, &current_block->code_section, left_operand_id, right_operand_id);
        return result_should_be_pointer ? left_operand_id : right_operand_id;
    }

#define TRIVIAL_LOGICAL_OPERATION(OPERATION)                                                                           \
    {                                                                                                                  \
        BINARY_OPERATION_COMMON_PREPARE;                                                                               \
        uint32_t result_id = context->current_bound;                                                                   \
        ++context->current_bound;                                                                                      \
                                                                                                                       \
        uint32_t *code = spirv_new_instruction (context, &current_block->code_section, 5u);                            \
        code[0u] |= SpvOpCodeMask & OPERATION;                                                                         \
        code[1u] = (uint32_t) SPIRV_FIXED_ID_TYPE_BOOLEAN;                                                             \
        code[2u] = result_id;                                                                                          \
        code[3u] = left_operand_id;                                                                                    \
        code[4u] = right_operand_id;                                                                                   \
                                                                                                                       \
        KAN_ASSERT (!result_should_be_pointer)                                                                         \
        return result_id;                                                                                              \
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_AND:
        TRIVIAL_LOGICAL_OPERATION (SpvOpLogicalAnd)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_OR:
        TRIVIAL_LOGICAL_OPERATION (SpvOpLogicalOr)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_EQUAL:
        TRIVIAL_LOGICAL_OPERATION (SpvOpIEqual)

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NOT_EQUAL:
        TRIVIAL_LOGICAL_OPERATION (SpvOpINotEqual)

#define SCALAR_LOGICAL_OPERATION(WHEN_FLOAT, WHEN_INTEGER)                                                             \
    {                                                                                                                  \
        BINARY_OPERATION_COMMON_PREPARE;                                                                               \
        uint32_t result_id = context->current_bound;                                                                   \
        ++context->current_bound;                                                                                      \
                                                                                                                       \
        uint32_t operation;                                                                                            \
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
        uint32_t *code = spirv_new_instruction (context, &current_block->code_section, 5u);                            \
        code[0u] |= SpvOpCodeMask & operation;                                                                         \
        code[1u] = (uint32_t) SPIRV_FIXED_ID_TYPE_BOOLEAN;                                                             \
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
        uint32_t result_id = context->current_bound;                                                                   \
        ++context->current_bound;                                                                                      \
                                                                                                                       \
        uint32_t *code = spirv_new_instruction (context, &current_block->code_section, 5u);                            \
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
        uint32_t operand_id =
            spirv_emit_expression (context, function, current_block, expression->unary_operation.operand, KAN_FALSE);

        uint32_t result_id = context->current_bound;
        ++context->current_bound;

        if (expression->output.type.if_vector)
        {
            uint32_t operation;
            switch (expression->output.type.if_vector->item)
            {
            case INBUILT_TYPE_ITEM_FLOAT:
                operation = SpvOpFNegate;
                break;

            case INBUILT_TYPE_ITEM_INTEGER:
                operation = SpvOpSNegate;
                break;
            }

            uint32_t *code = spirv_new_instruction (context, &current_block->code_section, 4u);
            code[0u] |= SpvOpCodeMask & operation;
            code[1u] = expression->output.type.if_vector->spirv_id;
            code[2u] = result_id;
            code[3u] = operand_id;
        }
        else if (expression->output.type.if_matrix)
        {
            uint32_t constant_id;
            switch (expression->output.type.if_matrix->item)
            {
            case INBUILT_TYPE_ITEM_FLOAT:
                constant_id = spirv_request_f1_constant (context, -1.0f);
                break;

            case INBUILT_TYPE_ITEM_INTEGER:
                constant_id = spirv_request_i1_constant (context, -1);
                break;
            }

            uint32_t *multiply = spirv_new_instruction (context, &current_block->code_section, 5u);
            multiply[0u] |= SpvOpCodeMask & SpvOpMatrixTimesScalar;
            multiply[1u] |= expression->output.type.if_matrix->spirv_id;
            multiply[2u] |= result_id;
            multiply[3u] |= operand_id;
            multiply[4u] |= constant_id;
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
        uint32_t operand_id =
            spirv_emit_expression (context, function, current_block, expression->unary_operation.operand, KAN_FALSE);

        uint32_t result_id = context->current_bound;
        ++context->current_bound;

        uint32_t *code = spirv_new_instruction (context, &current_block->code_section, 4u);
        code[0u] |= SpvOpCodeMask & SpvOpLogicalNot;
        code[1u] = (uint32_t) SPIRV_FIXED_ID_TYPE_BOOLEAN;
        code[2u] = result_id;
        code[3u] = operand_id;

        KAN_ASSERT (!result_should_be_pointer)
        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_NOT:
    {
        uint32_t operand_id =
            spirv_emit_expression (context, function, current_block, expression->unary_operation.operand, KAN_FALSE);

        uint32_t result_id = context->current_bound;
        ++context->current_bound;

        uint32_t *code = spirv_new_instruction (context, &current_block->code_section, 4u);
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
            uint32_t result_id = spirv_emit_inbuilt_function_call (context, function, current_block, expression);
            if (expression->function_call.function->return_type_if_vector ||
                expression->function_call.function->return_type_if_matrix ||
                expression->function_call.function->return_type_if_struct)
            {
                WRAP_OPERATION_RESULT_IF_NEEDED
            }

            return result_id;
        }

        uint64_t argument_count = 0u;
        struct compiler_instance_expression_list_item_t *argument = expression->function_call.first_argument;

        while (argument)
        {
            ++argument_count;
            argument = argument->next;
        }

        uint32_t *arguments = NULL;
        if (argument_count > 0u)
        {
            arguments = kan_stack_group_allocator_allocate (&context->temporary_allocator,
                                                            sizeof (uint32_t) * argument_count, _Alignof (uint32_t));
            uint64_t argument_index = 0u;
            argument = expression->function_call.first_argument;

            while (argument)
            {
                arguments[argument_index] =
                    spirv_emit_expression (context, function, current_block, argument->expression, KAN_TRUE);
                ++argument_index;
                argument = argument->next;
            }
        }

        uint32_t result_id = context->current_bound;
        ++context->current_bound;

        uint32_t *code = spirv_new_instruction (context, &current_block->code_section, 4u + argument_count);
        code[0u] |= SpvOpCodeMask & SpvOpFunctionCall;
        code[1u] = expression->function_call.function->spirv_function_type->return_type_id;
        code[2u] = result_id;
        code[3u] = expression->function_call.function->spirv_id;

        if (arguments)
        {
            memcpy (code + 4u, arguments, sizeof (uint32_t) * argument_count);
        }

        if (expression->function_call.function->spirv_function_type->return_type_id !=
            (uint32_t) SPIRV_FIXED_ID_TYPE_VOID)
        {
            WRAP_OPERATION_RESULT_IF_NEEDED
        }

        return result_id;
    }

    case COMPILER_INSTANCE_EXPRESSION_TYPE_SAMPLER_CALL:
        // TODO: Implement.
        return (uint32_t) SPIRV_FIXED_ID_INVALID;

    case COMPILER_INSTANCE_EXPRESSION_TYPE_CONSTRUCTOR:
        // TODO: Implement.
        return (uint32_t) SPIRV_FIXED_ID_INVALID;

    case COMPILER_INSTANCE_EXPRESSION_TYPE_SCOPE:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_IF:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_FOR:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_WHILE:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_BREAK:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_CONTINUE:
    case COMPILER_INSTANCE_EXPRESSION_TYPE_RETURN:
        // Should be processes along statements.
        KAN_ASSERT (KAN_FALSE)
        return (uint32_t) SPIRV_FIXED_ID_INVALID;
    }

#undef WRAP_OPERATION_RESULT_IF_NEEDED
#undef BINARY_OPERATION_COMMON_PREPARE
#undef TRIVIAL_LOGICAL_OPERATION
#undef SCALAR_LOGICAL_OPERATION
#undef TRIVIAL_BITWISE_OPERATION

    KAN_ASSERT (KAN_FALSE)
    return (uint32_t) SPIRV_FIXED_ID_INVALID;
}

static void spirv_emit_scope (struct spirv_generation_context_t *context,
                              struct spirv_generation_function_node_t *function,
                              struct spirv_generation_block_t *current_block,
                              struct spirv_generation_block_t *next_block,
                              struct compiler_instance_expression_node_t *scope_expression)
{
    KAN_ASSERT (scope_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_SCOPE)
    struct compiler_instance_scope_variable_item_t *variable = scope_expression->scope.first_variable;

    while (variable)
    {
        variable->spirv_id = context->current_bound;
        ++context->current_bound;

        uint32_t *variable_code = spirv_new_instruction (context, &function->first_block->header_section, 4u);
        variable_code[0u] |= SpvOpCodeMask & SpvOpVariable;
        variable_code[1u] = spirv_find_or_generate_variable_type (context, &variable->variable->type, 0u, KAN_TRUE);
        variable_code[2u] = variable->spirv_id;
        variable_code[3u] = SpvStorageClassFunction;

        spirv_generate_op_name (context, variable->spirv_id, variable->variable->name);
        variable = variable->next;
    }

    struct compiler_instance_expression_list_item_t *statement = scope_expression->scope.first_expression;
    struct compiler_instance_expression_list_item_t *previous_statement = NULL;

    while (statement)
    {
        if (previous_statement && previous_statement->expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_RETURN)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Found expressions after function return, generated SPIRV will be invalid.",
                     context->instance->context_log_name, statement->expression->module_name,
                     statement->expression->source_name, (long) statement->expression->source_line)
            context->emit_result = KAN_FALSE;
            break;
        }

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
            uint32_t result_id =
                spirv_emit_expression (context, function, current_block, statement->expression, KAN_TRUE);
            spirv_if_temporary_variable_then_stop_using (function, result_id);
            break;
        }

        case COMPILER_INSTANCE_EXPRESSION_TYPE_SCOPE:
            // TODO: Implement.
            break;

        case COMPILER_INSTANCE_EXPRESSION_TYPE_IF:
            // TODO: Implement.
            break;

        case COMPILER_INSTANCE_EXPRESSION_TYPE_FOR:
            // TODO: Implement.
            break;

        case COMPILER_INSTANCE_EXPRESSION_TYPE_WHILE:
            // TODO: Implement.
            break;

        case COMPILER_INSTANCE_EXPRESSION_TYPE_BREAK:
            // TODO: Implement.
            break;

        case COMPILER_INSTANCE_EXPRESSION_TYPE_CONTINUE:
            // TODO: Implement.
            break;

        case COMPILER_INSTANCE_EXPRESSION_TYPE_RETURN:
            // TODO: Implement.
            break;
        }

        previous_statement = statement;
        statement = statement->next;
    }

    const kan_bool_t emitted_return =
        previous_statement && previous_statement->expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_RETURN;

    if (next_block && !emitted_return)
    {
        uint32_t *branch_code = spirv_new_instruction (context, &current_block->code_section, 1u);
        branch_code[0u] |= SpvOpCodeMask & SpvOpBranch;
        branch_code[1u] = next_block->spirv_id;
    }
    else if (!emitted_return)
    {
        if (function->source->return_type_if_vector || function->source->return_type_if_matrix ||
            function->source->return_type_if_struct)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Reached last statement of non-void function \"%s\" execution loop and it is not a "
                     "return.",
                     context->instance->context_log_name,
                     previous_statement ? previous_statement->expression->module_name : "<node>",
                     previous_statement ? previous_statement->expression->source_name : "<node>",
                     (long) (previous_statement ? previous_statement->expression->source_line : 0u),
                     function->source->name)
            context->emit_result = KAN_FALSE;
        }
        else
        {
            // Emit missing return.
            uint32_t *return_code = spirv_new_instruction (context, &current_block->code_section, 1u);
            return_code[0u] |= SpvOpCodeMask & SpvOpReturn;
        }
    }
}

static inline void spirv_emit_function (struct spirv_generation_context_t *context,
                                        struct compiler_instance_function_node_t *function)
{
    struct spirv_generation_function_node_t *generated_function = kan_stack_group_allocator_allocate (
        &context->temporary_allocator, sizeof (struct spirv_generation_function_node_t),
        _Alignof (struct spirv_generation_function_node_t));

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

    uint32_t *definition_code = spirv_new_instruction (context, &generated_function->header_section, 5u);
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
        case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
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

        uint32_t *argument_code = spirv_new_instruction (context, &generated_function->header_section, 3u);
        argument_code[0u] |= SpvOpCodeMask & SpvOpFunctionParameter;
        argument_code[1u] =
            spirv_find_or_generate_variable_type (context, &argument_variable->variable->type, 0u, KAN_TRUE);
        argument_code[2u] = argument_variable->spirv_id;

        spirv_generate_op_name (context, argument_variable->spirv_id, argument_variable->variable->name);
        argument_variable = argument_variable->next;
    }

    uint32_t *end_code = spirv_new_instruction (context, &generated_function->end_section, 1u);
    end_code[0u] |= SpvOpCodeMask & SpvOpFunctionEnd;

    struct spirv_generation_block_t *first_block = spirv_function_new_block (context, generated_function);
    spirv_emit_scope (context, generated_function, first_block, NULL, function->body);
}

kan_bool_t kan_rpl_compiler_instance_emit_spirv (kan_rpl_compiler_instance_t compiler_instance,
                                                 struct kan_dynamic_array_t *output,
                                                 kan_allocation_group_t output_allocation_group)
{
    kan_dynamic_array_init (output, 0u, sizeof (uint32_t), _Alignof (uint32_t), output_allocation_group);
    struct rpl_compiler_instance_t *instance = (struct rpl_compiler_instance_t *) compiler_instance;
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

        uint32_t *pointer_code = spirv_new_instruction (&context, &context.higher_type_section, 4u);
        pointer_code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
        pointer_code[1u] = struct_node->spirv_id_function_pointer;
        pointer_code[2u] = SpvStorageClassFunction;
        pointer_code[3u] = struct_node->spirv_id_value;

        spirv_generate_op_name (&context, struct_node->spirv_id_function_pointer, struct_node->name);
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
                    spirv_emit_flattened_input_variable (&context, declaration);
                    break;

                case KAN_RPL_BUFFER_TYPE_UNIFORM:
                case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
                case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
                case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
                    KAN_ASSERT (KAN_FALSE)
                    break;

                case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
                    spirv_emit_flattened_input_variable (&context, declaration);
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
            uint32_t buffer_struct_id = context.current_bound;
            ++context.current_bound;
            spirv_emit_struct_from_declaration_list (&context, buffer->first_field, buffer->name, buffer_struct_id);

            if (buffer->type == KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM ||
                buffer->type == KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE)
            {
                uint32_t runtime_array_id = context.current_bound;
                ++context.current_bound;

                uint32_t *runtime_array_code = spirv_new_instruction (&context, &context.higher_type_section, 3u);
                runtime_array_code[0u] |= SpvOpCodeMask & SpvOpTypeRuntimeArray;
                runtime_array_code[1u] = runtime_array_id;
                runtime_array_code[2u] = buffer_struct_id;

                uint32_t real_buffer_struct_id = context.current_bound;
                ++context.current_bound;

                uint32_t *wrapper_struct_code = spirv_new_instruction (&context, &context.higher_type_section, 3u);
                wrapper_struct_code[0u] |= SpvOpCodeMask & SpvOpTypeStruct;
                wrapper_struct_code[1u] = real_buffer_struct_id;
                wrapper_struct_code[2u] = runtime_array_id;

                uint32_t *wrapper_decorate_code = spirv_new_instruction (&context, &context.decoration_section, 3u);
                wrapper_decorate_code[0u] |= SpvOpCodeMask & SpvOpDecorate;
                wrapper_decorate_code[1u] = real_buffer_struct_id;
                wrapper_decorate_code[2u] = SpvDecorationBlock;

                spirv_generate_op_name (&context, real_buffer_struct_id, buffer->name);
                spirv_generate_op_member_name (&context, real_buffer_struct_id, 0u, "instanced_data");
                buffer_struct_id = real_buffer_struct_id;
            }

            uint32_t storage_type = UINT32_MAX;
            switch (buffer->type)
            {
            case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
            case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
            case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
                KAN_ASSERT (KAN_FALSE)
                break;

            case KAN_RPL_BUFFER_TYPE_UNIFORM:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
                storage_type = SpvStorageClassUniformConstant;
                break;

            case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
                storage_type = SpvStorageClassStorageBuffer;
                break;
            }

            uint32_t buffer_struct_pointer_id = context.current_bound;
            ++context.current_bound;

            uint32_t *pointer_code = spirv_new_instruction (&context, &context.higher_type_section, 4u);
            pointer_code[0u] |= SpvOpCodeMask & SpvOpTypePointer;
            pointer_code[1u] = buffer_struct_pointer_id;
            pointer_code[2u] = storage_type;
            pointer_code[3u] = buffer_struct_id;

            buffer->structured_variable_spirv_id = context.current_bound;
            ++context.current_bound;

            uint32_t *variable_code = spirv_new_instruction (&context, &context.global_variable_section, 4u);
            variable_code[0u] |= SpvOpCodeMask & SpvOpVariable;
            variable_code[1u] = buffer_struct_pointer_id;
            variable_code[2u] = buffer->structured_variable_spirv_id;
            variable_code[3u] = storage_type;

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

        uint32_t *sampler_code = spirv_new_instruction (&context, &context.global_variable_section, 4u);
        sampler_code[0u] |= SpvOpCodeMask & SpvOpVariable;

        switch (sampler->type)
        {
        case KAN_RPL_SAMPLER_TYPE_2D:
            sampler_code[1u] = SPIRV_FIXED_ID_TYPE_SAMPLER_2D_POINTER;
            break;
        }

        sampler_code[2u] = sampler->variable_spirv_id;
        sampler_code[3u] = SpvStorageClassUniformConstant;

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
