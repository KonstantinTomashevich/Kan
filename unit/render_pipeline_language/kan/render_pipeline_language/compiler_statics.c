#include <spirv/unified1/GLSL.std.450.h>

#define KAN_RPL_COMPILER_IMPLEMENTATION
#include <kan/render_pipeline_language/compiler_internal.h>

static kan_bool_t statics_initialized = KAN_FALSE;
struct kan_rpl_compiler_statics_t kan_rpl_compiler_statics;

static void build_repeating_vector_constructor_signatures (
    struct inbuilt_vector_type_t *item,
    uint64_t repeats,
    struct compiler_instance_declaration_node_t *declaration_array_output)
{
    char name[3u] = "_0";
    for (uint64_t index = 0u; index < repeats; ++index)
    {
        name[1u] = (char) ('0' + index);
        declaration_array_output[index] = (struct compiler_instance_declaration_node_t) {
            .next = index + 1u == repeats ? NULL : &declaration_array_output[index + 1u],
            .variable = {.name = kan_string_intern (name),
                         .type =
                             {
                                 .if_vector = item,
                                 .if_matrix = NULL,
                                 .if_struct = NULL,
                                 .array_dimensions_count = 0u,
                                 .array_dimensions = NULL,
                             }},
            .meta_count = 0u,
            .meta = NULL,
            .module_name = NULL,
            .source_name = NULL,
            .source_line = 0u,
        };
    }
}

void kan_rpl_compiler_ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        STATICS.rpl_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "render_pipeline_language");
        STATICS.rpl_meta_allocation_group = kan_allocation_group_get_child (STATICS.rpl_allocation_group, "meta");
        STATICS.rpl_compiler_allocation_group =
            kan_allocation_group_get_child (STATICS.rpl_allocation_group, "compiler");
        STATICS.rpl_compiler_builtin_hash_allocation_group =
            kan_allocation_group_get_child (STATICS.rpl_compiler_allocation_group, "builtin_hash");
        STATICS.rpl_compiler_context_allocation_group =
            kan_allocation_group_get_child (STATICS.rpl_compiler_allocation_group, "context");
        STATICS.rpl_compiler_instance_allocation_group =
            kan_allocation_group_get_child (STATICS.rpl_compiler_allocation_group, "instance");

        STATICS.interned_fill = kan_string_intern ("fill");
        STATICS.interned_wireframe = kan_string_intern ("wireframe");
        STATICS.interned_back = kan_string_intern ("back");

        STATICS.interned_never = kan_string_intern ("never");
        STATICS.interned_always = kan_string_intern ("always");
        STATICS.interned_equal = kan_string_intern ("equal");
        STATICS.interned_not_equal = kan_string_intern ("not_equal");
        STATICS.interned_less = kan_string_intern ("less");
        STATICS.interned_less_or_equal = kan_string_intern ("less_or_equal");
        STATICS.interned_greater = kan_string_intern ("greater");
        STATICS.interned_greater_or_equal = kan_string_intern ("greater_or_equal");
        
        STATICS.interned_keep = kan_string_intern ("keep");
        STATICS.interned_replace = kan_string_intern ("replace");
        STATICS.interned_increment_and_clamp = kan_string_intern ("increment_and_clamp");
        STATICS.interned_decrement_and_clamp = kan_string_intern ("decrement_and_clamp");
        STATICS.interned_invert = kan_string_intern ("invert");
        STATICS.interned_increment_and_wrap = kan_string_intern ("increment_and_wrap");
        STATICS.interned_decrement_and_wrap = kan_string_intern ("decrement_and_wrap");

        STATICS.interned_polygon_mode = kan_string_intern ("polygon_mode");
        STATICS.interned_cull_mode = kan_string_intern ("cull_mode");
        STATICS.interned_depth_test = kan_string_intern ("depth_test");
        STATICS.interned_depth_write = kan_string_intern ("depth_write");
        STATICS.interned_depth_bounds_test = kan_string_intern ("depth_bounds_test");
        STATICS.interned_depth_compare_operation = kan_string_intern ("depth_compare_operation");
        STATICS.interned_depth_min = kan_string_intern ("depth_min");
        STATICS.interned_depth_max = kan_string_intern ("depth_max");
        STATICS.interned_stencil_test = kan_string_intern ("stencil_test");
        STATICS.interned_stencil_front_on_fail = kan_string_intern ("stencil_front_on_fail");
        STATICS.interned_stencil_front_on_depth_fail = kan_string_intern ("stencil_front_on_depth_fail");
        STATICS.interned_stencil_front_on_pass = kan_string_intern ("stencil_front_on_pass");
        STATICS.interned_stencil_front_compare = kan_string_intern ("stencil_front_compare");
        STATICS.interned_stencil_front_compare_mask = kan_string_intern ("stencil_front_compare_mask");
        STATICS.interned_stencil_front_write_mask = kan_string_intern ("stencil_front_write_mask");
        STATICS.interned_stencil_front_reference = kan_string_intern ("stencil_front_reference");
        STATICS.interned_stencil_back_on_fail = kan_string_intern ("stencil_back_on_fail");
        STATICS.interned_stencil_back_on_depth_fail = kan_string_intern ("stencil_back_on_depth_fail");
        STATICS.interned_stencil_back_on_pass = kan_string_intern ("stencil_back_on_pass");
        STATICS.interned_stencil_back_compare = kan_string_intern ("stencil_back_compare");
        STATICS.interned_stencil_back_compare_mask = kan_string_intern ("stencil_back_compare_mask");
        STATICS.interned_stencil_back_write_mask = kan_string_intern ("stencil_back_write_mask");
        STATICS.interned_stencil_back_reference = kan_string_intern ("stencil_back_reference");

        STATICS.interned_nearest = kan_string_intern ("nearest");
        STATICS.interned_linear = kan_string_intern ("linear");
        STATICS.interned_repeat = kan_string_intern ("repeat");
        STATICS.interned_mirrored_repeat = kan_string_intern ("mirrored_repeat");
        STATICS.interned_clamp_to_edge = kan_string_intern ("clamp_to_edge");
        STATICS.interned_clamp_to_border = kan_string_intern ("clamp_to_border");
        STATICS.interned_mirror_clamp_to_edge = kan_string_intern ("mirror_clamp_to_edge");
        STATICS.interned_mirror_clamp_to_border = kan_string_intern ("mirror_clamp_to_border");

        STATICS.interned_mag_filter = kan_string_intern ("mag_filter");
        STATICS.interned_min_filter = kan_string_intern ("min_filter");
        STATICS.interned_mip_map_mode = kan_string_intern ("mip_map_mode");
        STATICS.interned_address_mode_u = kan_string_intern ("address_mode_u");
        STATICS.interned_address_mode_v = kan_string_intern ("address_mode_v");
        STATICS.interned_address_mode_w = kan_string_intern ("address_mode_w");

        STATICS.interned_zero = kan_string_intern ("zero");
        STATICS.interned_one = kan_string_intern ("one");
        STATICS.interned_source_color = kan_string_intern ("source_color");
        STATICS.interned_one_minus_source_color = kan_string_intern ("one_minus_source_color");
        STATICS.interned_destination_color = kan_string_intern ("destination_color");
        STATICS.interned_one_minus_destination_color = kan_string_intern ("one_minus_destination_color");
        STATICS.interned_source_alpha = kan_string_intern ("source_alpha");
        STATICS.interned_one_minus_source_alpha = kan_string_intern ("one_minus_source_alpha");
        STATICS.interned_destination_alpha = kan_string_intern ("destination_alpha");
        STATICS.interned_one_minus_destination_alpha = kan_string_intern ("one_minus_destination_alpha");
        STATICS.interned_constant_color = kan_string_intern ("constant_color");
        STATICS.interned_one_minus_constant_color = kan_string_intern ("one_minus_constant_color");
        STATICS.interned_constant_alpha = kan_string_intern ("constant_alpha");
        STATICS.interned_one_minus_constant_alpha = kan_string_intern ("one_minus_constant_alpha");
        STATICS.interned_source_alpha_saturate = kan_string_intern ("source_alpha_saturate");

        STATICS.interned_add = kan_string_intern ("add");
        STATICS.interned_subtract = kan_string_intern ("subtract");
        STATICS.interned_reverse_subtract = kan_string_intern ("reverse_subtract");
        STATICS.interned_min = kan_string_intern ("min");
        STATICS.interned_max = kan_string_intern ("max");

        STATICS.interned_color_output_use_blend = kan_string_intern ("color_output_use_blend");
        STATICS.interned_color_output_write_r = kan_string_intern ("color_output_write_r");
        STATICS.interned_color_output_write_g = kan_string_intern ("color_output_write_g");
        STATICS.interned_color_output_write_b = kan_string_intern ("color_output_write_b");
        STATICS.interned_color_output_write_a = kan_string_intern ("color_output_write_a");
        STATICS.interned_color_output_source_color_blend_factor =
            kan_string_intern ("color_output_source_color_blend_factor");
        STATICS.interned_color_output_destination_color_blend_factor =
            kan_string_intern ("color_output_destination_color_blend_factor");
        STATICS.interned_color_output_color_blend_operation = kan_string_intern ("color_output_color_blend_operation");
        STATICS.interned_color_output_source_alpha_blend_factor =
            kan_string_intern ("color_output_source_alpha_blend_factor");
        STATICS.interned_color_output_destination_alpha_blend_factor =
            kan_string_intern ("color_output_destination_alpha_blend_factor");
        STATICS.interned_color_output_alpha_blend_operation = kan_string_intern ("color_output_alpha_blend_operation");

        STATICS.interned_color_blend_constant_r = kan_string_intern ("color_blend_constant_r");
        STATICS.interned_color_blend_constant_g = kan_string_intern ("color_blend_constant_g");
        STATICS.interned_color_blend_constant_b = kan_string_intern ("color_blend_constant_b");
        STATICS.interned_color_blend_constant_a = kan_string_intern ("color_blend_constant_a");

        STATICS.interned_void = kan_string_intern ("void");

        STATICS.type_f1 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("f1"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .items_count = 1u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F1,
            .constructor_signature = STATICS.type_f1_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_FLOAT,
            .spirv_id_input_pointer = SPIRV_FIXED_ID_TYPE_FLOAT_INPUT_POINTER,
            .spirv_id_output_pointer = SPIRV_FIXED_ID_TYPE_FLOAT_OUTPUT_POINTER,
            .spirv_id_function_pointer = SPIRV_FIXED_ID_TYPE_FLOAT_FUNCTION_POINTER,
        };

        STATICS.type_f2 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("f2"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .items_count = 2u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F2,
            .constructor_signature = STATICS.type_f2_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_F2,
            .spirv_id_input_pointer = SPIRV_FIXED_ID_TYPE_F2_INPUT_POINTER,
            .spirv_id_output_pointer = SPIRV_FIXED_ID_TYPE_F2_OUTPUT_POINTER,
            .spirv_id_function_pointer = SPIRV_FIXED_ID_TYPE_F2_FUNCTION_POINTER,
        };

        STATICS.type_f3 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("f3"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .items_count = 3u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F3,
            .constructor_signature = STATICS.type_f3_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_F3,
            .spirv_id_input_pointer = SPIRV_FIXED_ID_TYPE_F3_INPUT_POINTER,
            .spirv_id_output_pointer = SPIRV_FIXED_ID_TYPE_F3_OUTPUT_POINTER,
            .spirv_id_function_pointer = SPIRV_FIXED_ID_TYPE_F3_FUNCTION_POINTER,
        };

        STATICS.type_f4 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("f4"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .items_count = 4u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F4,
            .constructor_signature = STATICS.type_f4_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_F4,
            .spirv_id_input_pointer = SPIRV_FIXED_ID_TYPE_F4_INPUT_POINTER,
            .spirv_id_output_pointer = SPIRV_FIXED_ID_TYPE_F4_OUTPUT_POINTER,
            .spirv_id_function_pointer = SPIRV_FIXED_ID_TYPE_F4_FUNCTION_POINTER,
        };

        STATICS.type_i1 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("i1"),
            .item = INBUILT_TYPE_ITEM_INTEGER,
            .items_count = 1u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_I1,
            .constructor_signature = STATICS.type_i1_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_INTEGER,
            .spirv_id_input_pointer = SPIRV_FIXED_ID_TYPE_INTEGER_INPUT_POINTER,
            .spirv_id_output_pointer = SPIRV_FIXED_ID_TYPE_INTEGER_OUTPUT_POINTER,
            .spirv_id_function_pointer = SPIRV_FIXED_ID_TYPE_INTEGER_OUTPUT_POINTER,
        };

        STATICS.type_i2 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("i2"),
            .item = INBUILT_TYPE_ITEM_INTEGER,
            .items_count = 2u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_I2,
            .constructor_signature = STATICS.type_i2_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_I2,
            .spirv_id_input_pointer = SPIRV_FIXED_ID_TYPE_I2_INPUT_POINTER,
            .spirv_id_output_pointer = SPIRV_FIXED_ID_TYPE_I2_OUTPUT_POINTER,
            .spirv_id_function_pointer = SPIRV_FIXED_ID_TYPE_I2_OUTPUT_POINTER,
        };

        STATICS.type_i3 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("i3"),
            .item = INBUILT_TYPE_ITEM_INTEGER,
            .items_count = 3u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_I3,
            .constructor_signature = STATICS.type_i3_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_I3,
            .spirv_id_input_pointer = SPIRV_FIXED_ID_TYPE_I3_INPUT_POINTER,
            .spirv_id_output_pointer = SPIRV_FIXED_ID_TYPE_I3_OUTPUT_POINTER,
            .spirv_id_function_pointer = SPIRV_FIXED_ID_TYPE_I3_OUTPUT_POINTER,
        };

        STATICS.type_i4 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("i4"),
            .item = INBUILT_TYPE_ITEM_INTEGER,
            .items_count = 4u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_I4,
            .constructor_signature = STATICS.type_i4_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_I4,
            .spirv_id_input_pointer = SPIRV_FIXED_ID_TYPE_I4_INPUT_POINTER,
            .spirv_id_output_pointer = SPIRV_FIXED_ID_TYPE_I4_OUTPUT_POINTER,
            .spirv_id_function_pointer = SPIRV_FIXED_ID_TYPE_I4_OUTPUT_POINTER,
        };

        STATICS.type_f3x3 = (struct inbuilt_matrix_type_t) {
            .name = kan_string_intern ("f3x3"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .rows = 3u,
            .columns = 3u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F3X3,
            .constructor_signature = STATICS.type_f3x3_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_F3X3,
            .spirv_id_input_pointer = SPIRV_FIXED_ID_TYPE_F3X3_INPUT_POINTER,
            .spirv_id_output_pointer = SPIRV_FIXED_ID_TYPE_F3X3_OUTPUT_POINTER,
            .spirv_id_function_pointer = SPIRV_FIXED_ID_TYPE_F3X3_OUTPUT_POINTER,
        };

        STATICS.type_f4x4 = (struct inbuilt_matrix_type_t) {
            .name = kan_string_intern ("f4x4"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .rows = 4u,
            .columns = 4u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F4X4,
            .constructor_signature = STATICS.type_f4x4_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_F4X4,
            .spirv_id_input_pointer = SPIRV_FIXED_ID_TYPE_F4X4_INPUT_POINTER,
            .spirv_id_output_pointer = SPIRV_FIXED_ID_TYPE_F4X4_OUTPUT_POINTER,
            .spirv_id_function_pointer = SPIRV_FIXED_ID_TYPE_F4X4_OUTPUT_POINTER,
        };

        build_repeating_vector_constructor_signatures (&STATICS.type_f1, 1u, STATICS.type_f1_constructor_signatures);
        build_repeating_vector_constructor_signatures (&STATICS.type_f1, 2u, STATICS.type_f2_constructor_signatures);
        build_repeating_vector_constructor_signatures (&STATICS.type_f1, 3u, STATICS.type_f3_constructor_signatures);
        build_repeating_vector_constructor_signatures (&STATICS.type_f1, 4u, STATICS.type_f4_constructor_signatures);
        build_repeating_vector_constructor_signatures (&STATICS.type_i1, 1u, STATICS.type_i1_constructor_signatures);
        build_repeating_vector_constructor_signatures (&STATICS.type_i1, 2u, STATICS.type_i2_constructor_signatures);
        build_repeating_vector_constructor_signatures (&STATICS.type_i1, 3u, STATICS.type_i3_constructor_signatures);
        build_repeating_vector_constructor_signatures (&STATICS.type_i1, 4u, STATICS.type_i4_constructor_signatures);
        build_repeating_vector_constructor_signatures (&STATICS.type_f3, 3u, STATICS.type_f3x3_constructor_signatures);
        build_repeating_vector_constructor_signatures (&STATICS.type_f4, 4u, STATICS.type_f4x4_constructor_signatures);

        STATICS.vector_types[0u] = &STATICS.type_f1;
        STATICS.vector_types[1u] = &STATICS.type_f2;
        STATICS.vector_types[2u] = &STATICS.type_f3;
        STATICS.vector_types[3u] = &STATICS.type_f4;
        STATICS.vector_types[4u] = &STATICS.type_i1;
        STATICS.vector_types[5u] = &STATICS.type_i2;
        STATICS.vector_types[6u] = &STATICS.type_i3;
        STATICS.vector_types[7u] = &STATICS.type_i4;

        STATICS.floating_vector_types[0u] = &STATICS.type_f1;
        STATICS.floating_vector_types[1u] = &STATICS.type_f2;
        STATICS.floating_vector_types[2u] = &STATICS.type_f3;
        STATICS.floating_vector_types[3u] = &STATICS.type_f4;

        STATICS.integer_vector_types[0u] = &STATICS.type_i1;
        STATICS.integer_vector_types[1u] = &STATICS.type_i2;
        STATICS.integer_vector_types[2u] = &STATICS.type_i3;
        STATICS.integer_vector_types[3u] = &STATICS.type_i4;

        STATICS.matrix_types[0u] = &STATICS.type_f3x3;
        STATICS.matrix_types[1u] = &STATICS.type_f4x4;

        const kan_interned_string_t interned_sampler = kan_string_intern ("sampler");
        const kan_interned_string_t interned_calls = kan_string_intern ("calls");

        STATICS.sampler_2d_call_signature_first_element = &STATICS.sampler_2d_call_signature_location;
        STATICS.sampler_2d_call_signature_location = (struct compiler_instance_declaration_node_t) {
            .next = NULL,
            .variable = {.name = kan_string_intern ("location"),
                         .type =
                             {
                                 .if_vector = &STATICS.type_f2,
                                 .if_matrix = NULL,
                                 .if_struct = NULL,
                                 .array_dimensions_count = 0u,
                                 .array_dimensions = NULL,
                             }},
            .meta_count = 0u,
            .meta = NULL,
            .module_name = interned_sampler,
            .source_name = interned_calls,
            .source_line = 0u,
        };

        const kan_interned_string_t module_standard = kan_string_intern ("standard");
        const kan_interned_string_t source_functions = kan_string_intern ("functions");

        kan_hash_storage_init (&STATICS.builtin_hash_storage, STATICS.rpl_compiler_builtin_hash_allocation_group,
                               KAN_RPL_BUILTIN_HASH_STORAGE_BUCKETS);

#define SPIRV_INTERNAL ((uint32_t) SPIRV_FIXED_ID_INVALID)

#define ANY_STAGE KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX

#define BUILTIN_COMMON(NAME, RETURN_IF_VECTOR, RETURN_IF_MATRIX, IS_STAGE_SPECIFIC, REQUIRED_STAGE,                    \
                       SPIRV_EXTERNAL_LIBRARY, SPIRV_EXTERNAL_INSTRUCTION, ARGUMENTS)                                  \
    STATICS.builtin_##NAME = (struct compiler_instance_function_node_t) {                                              \
        .next = NULL,                                                                                                  \
        .name = kan_string_intern (#NAME),                                                                             \
        .return_type_if_vector = RETURN_IF_VECTOR,                                                                     \
        .return_type_if_matrix = RETURN_IF_MATRIX,                                                                     \
        .return_type_if_struct = NULL,                                                                                 \
        .first_argument = ARGUMENTS,                                                                                   \
        .body = NULL,                                                                                                  \
        .has_stage_specific_access = IS_STAGE_SPECIFIC,                                                                \
        .required_stage = REQUIRED_STAGE,                                                                              \
        .first_buffer_access = NULL,                                                                                   \
        .first_sampler_access = NULL,                                                                                  \
        .spirv_external_library_id = (uint32_t) SPIRV_EXTERNAL_LIBRARY,                                                \
        .spirv_external_instruction_id = (uint32_t) SPIRV_EXTERNAL_INSTRUCTION,                                        \
        .module_name = module_standard,                                                                                \
        .source_name = source_functions,                                                                               \
        .source_line = 0u,                                                                                             \
    };                                                                                                                 \
                                                                                                                       \
    struct kan_rpl_compiler_builtin_node_t *node_##NAME = kan_allocate_batched (                                       \
        STATICS.rpl_compiler_builtin_hash_allocation_group, sizeof (struct kan_rpl_compiler_builtin_node_t));          \
    node_##NAME->node.hash = (uint64_t) STATICS.builtin_##NAME.name;                                                   \
    node_##NAME->builtin = &STATICS.builtin_##NAME;                                                                    \
    kan_hash_storage_add (&STATICS.builtin_hash_storage, &node_##NAME->node)

#define BUILTIN_ARGUMENT(BULTIN, INDEX, NEXT, NAME, IF_VECTOR, IF_MATRIX)                                              \
    STATICS.builtin_##BULTIN##_arguments[INDEX] = (struct compiler_instance_declaration_node_t) {                      \
        .next = NEXT,                                                                                                  \
        .variable = {.name = kan_string_intern (NAME),                                                                 \
                     .type =                                                                                           \
                         {                                                                                             \
                             .if_vector = IF_VECTOR,                                                                   \
                             .if_matrix = IF_MATRIX,                                                                   \
                             .if_struct = NULL,                                                                        \
                             .array_dimensions_count = 0u,                                                             \
                             .array_dimensions = NULL,                                                                 \
                         }},                                                                                           \
        .meta_count = 0u,                                                                                              \
        .meta = NULL,                                                                                                  \
        .module_name = module_standard,                                                                                \
        .source_name = source_functions,                                                                               \
        .source_line = 0u,                                                                                             \
    }

#define BUILTIN_0(NAME, RETURN_IF_VECTOR, RETURN_IF_MATRIX, IS_STAGE_SPECIFIC, REQUIRED_STAGE, SPIRV_EXTERNAL_LIBRARY, \
                  SPIRV_EXTERNAL_INSTRUCTION)                                                                          \
    BUILTIN_COMMON (NAME, RETURN_IF_VECTOR, RETURN_IF_MATRIX, IS_STAGE_SPECIFIC, REQUIRED_STAGE,                       \
                    SPIRV_EXTERNAL_LIBRARY, SPIRV_EXTERNAL_INSTRUCTION, NULL)

#define BUILTIN_1(NAME, RETURN_IF_VECTOR, RETURN_IF_MATRIX, IS_STAGE_SPECIFIC, REQUIRED_STAGE, SPIRV_EXTERNAL_LIBRARY, \
                  SPIRV_EXTERNAL_INSTRUCTION, ARGUMENT_1_NAME, ARGUMENT_1_IF_VECTOR, ARGUMENT_1_IF_MATRIX)             \
    BUILTIN_ARGUMENT (NAME, 0u, NULL, ARGUMENT_1_NAME, ARGUMENT_1_IF_VECTOR, ARGUMENT_1_IF_MATRIX);                    \
    BUILTIN_COMMON (NAME, RETURN_IF_VECTOR, RETURN_IF_MATRIX, IS_STAGE_SPECIFIC, REQUIRED_STAGE,                       \
                    SPIRV_EXTERNAL_LIBRARY, SPIRV_EXTERNAL_INSTRUCTION, STATICS.builtin_##NAME##_arguments)

#define BUILTIN_2(NAME, RETURN_IF_VECTOR, RETURN_IF_MATRIX, IS_STAGE_SPECIFIC, REQUIRED_STAGE, SPIRV_EXTERNAL_LIBRARY, \
                  SPIRV_EXTERNAL_INSTRUCTION, ARGUMENT_1_NAME, ARGUMENT_1_IF_VECTOR, ARGUMENT_1_IF_MATRIX,             \
                  ARGUMENT_2_NAME, ARGUMENT_2_IF_VECTOR, ARGUMENT_2_IF_MATRIX)                                         \
    BUILTIN_ARGUMENT (NAME, 0u, &STATICS.builtin_##NAME##_arguments[1u], ARGUMENT_1_NAME, ARGUMENT_1_IF_VECTOR,        \
                      ARGUMENT_1_IF_MATRIX);                                                                           \
    BUILTIN_ARGUMENT (NAME, 1u, NULL, ARGUMENT_2_NAME, ARGUMENT_2_IF_VECTOR, ARGUMENT_2_IF_MATRIX);                    \
    BUILTIN_COMMON (NAME, RETURN_IF_VECTOR, RETURN_IF_MATRIX, IS_STAGE_SPECIFIC, REQUIRED_STAGE,                       \
                    SPIRV_EXTERNAL_LIBRARY, SPIRV_EXTERNAL_INSTRUCTION, STATICS.builtin_##NAME##_arguments)

#define BUILTIN_3(NAME, RETURN_IF_VECTOR, RETURN_IF_MATRIX, IS_STAGE_SPECIFIC, REQUIRED_STAGE, SPIRV_EXTERNAL_LIBRARY, \
                  SPIRV_EXTERNAL_INSTRUCTION, ARGUMENT_1_NAME, ARGUMENT_1_IF_VECTOR, ARGUMENT_1_IF_MATRIX,             \
                  ARGUMENT_2_NAME, ARGUMENT_2_IF_VECTOR, ARGUMENT_2_IF_MATRIX, ARGUMENT_3_NAME, ARGUMENT_3_IF_VECTOR,  \
                  ARGUMENT_3_IF_MATRIX)                                                                                \
    BUILTIN_ARGUMENT (NAME, 0u, &STATICS.builtin_##NAME##_arguments[1u], ARGUMENT_1_NAME, ARGUMENT_1_IF_VECTOR,        \
                      ARGUMENT_1_IF_MATRIX);                                                                           \
    BUILTIN_ARGUMENT (NAME, 1u, &STATICS.builtin_##NAME##_arguments[2u], ARGUMENT_2_NAME, ARGUMENT_2_IF_VECTOR,        \
                      ARGUMENT_2_IF_MATRIX);                                                                           \
    BUILTIN_ARGUMENT (NAME, 2u, NULL, ARGUMENT_3_NAME, ARGUMENT_3_IF_VECTOR, ARGUMENT_3_IF_MATRIX);                    \
    BUILTIN_COMMON (NAME, RETURN_IF_VECTOR, RETURN_IF_MATRIX, IS_STAGE_SPECIFIC, REQUIRED_STAGE,                       \
                    SPIRV_EXTERNAL_LIBRARY, SPIRV_EXTERNAL_INSTRUCTION, STATICS.builtin_##NAME##_arguments)

        BUILTIN_1 (vertex_stage_output_position, NULL, NULL, KAN_TRUE, KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX,
                   SPIRV_INTERNAL, SPIRV_INTERNAL, "position", &STATICS.type_f4, NULL);

        BUILTIN_0 (pi, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL);

        BUILTIN_1 (i1_to_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL, "value",
                   &STATICS.type_i1, NULL);
        BUILTIN_1 (i2_to_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL, "value",
                   &STATICS.type_i2, NULL);
        BUILTIN_1 (i3_to_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL, "value",
                   &STATICS.type_i3, NULL);
        BUILTIN_1 (i4_to_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL, "value",
                   &STATICS.type_i4, NULL);

        BUILTIN_1 (f1_to_i1, &STATICS.type_i1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL, "value",
                   &STATICS.type_f1, NULL);
        BUILTIN_1 (f2_to_i2, &STATICS.type_i2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL, "value",
                   &STATICS.type_f2, NULL);
        BUILTIN_1 (f3_to_i3, &STATICS.type_i3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL, "value",
                   &STATICS.type_f3, NULL);
        BUILTIN_1 (f4_to_i4, &STATICS.type_i4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL, "value",
                   &STATICS.type_f4, NULL);

        BUILTIN_1 (round_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Round,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (round_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Round,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (round_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Round,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (round_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Round,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (round_even_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450RoundEven, "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (round_even_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450RoundEven, "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (round_even_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450RoundEven, "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (round_even_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450RoundEven, "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (trunc_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Trunc,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (trunc_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Trunc,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (trunc_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Trunc,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (trunc_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Trunc,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (abs_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FAbs,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (abs_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FAbs,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (abs_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FAbs,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (abs_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FAbs,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (abs_i1, &STATICS.type_i1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SAbs,
                   "value", &STATICS.type_i1, NULL);
        BUILTIN_1 (abs_i2, &STATICS.type_i2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SAbs,
                   "value", &STATICS.type_i2, NULL);
        BUILTIN_1 (abs_i3, &STATICS.type_i3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SAbs,
                   "value", &STATICS.type_i3, NULL);
        BUILTIN_1 (abs_i4, &STATICS.type_i4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SAbs,
                   "value", &STATICS.type_i4, NULL);

        BUILTIN_1 (sign_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FSign,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (sign_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FSign,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (sign_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FSign,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (sign_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FSign,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (sign_i1, &STATICS.type_i1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SSign,
                   "value", &STATICS.type_i1, NULL);
        BUILTIN_1 (sign_i2, &STATICS.type_i2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SSign,
                   "value", &STATICS.type_i2, NULL);
        BUILTIN_1 (sign_i3, &STATICS.type_i3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SSign,
                   "value", &STATICS.type_i3, NULL);
        BUILTIN_1 (sign_i4, &STATICS.type_i4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SSign,
                   "value", &STATICS.type_i4, NULL);

        BUILTIN_1 (floor_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Floor,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (floor_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Floor,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (floor_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Floor,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (floor_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Floor,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (ceil_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Ceil,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (ceil_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Ceil,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (ceil_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Ceil,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (ceil_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Ceil,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (fract_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Fract,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (fract_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Fract,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (fract_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Fract,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (fract_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Fract,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (sin_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Sin,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (sin_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Sin,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (sin_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Sin,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (sin_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Sin,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (cos_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Cos,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (cos_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Cos,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (cos_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Cos,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (cos_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Cos,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (tan_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Tan,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (tan_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Tan,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (tan_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Tan,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (tan_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Tan,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (asin_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Asin,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (asin_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Asin,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (asin_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Asin,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (asin_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Asin,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (acos_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Acos,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (acos_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Acos,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (acos_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Acos,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (acos_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Acos,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (atan_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Atan,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (atan_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Atan,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (atan_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Atan,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (atan_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Atan,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (sinh_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Sinh,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (sinh_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Sinh,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (sinh_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Sinh,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (sinh_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Sinh,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (cosh_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Cosh,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (cosh_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Cosh,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (cosh_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Cosh,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (cosh_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Cosh,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (tanh_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Tanh,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (tanh_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Tanh,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (tanh_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Tanh,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (tanh_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Tanh,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (asinh_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Asinh,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (asinh_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Asinh,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (asinh_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Asinh,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (asinh_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Asinh,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (acosh_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Acosh,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (acosh_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Acosh,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (acosh_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Acosh,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (acosh_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Acosh,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (atanh_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Atanh,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (atanh_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Atanh,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (atanh_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Atanh,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (atanh_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Atanh,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (atan2_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Atan2,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (atan2_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Atan2,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (atan2_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Atan2,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (atan2_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Atan2,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_2 (pow_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Pow,
                   "x", &STATICS.type_f1, NULL, "y", &STATICS.type_f1, NULL);
        BUILTIN_2 (pow_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Pow,
                   "x", &STATICS.type_f2, NULL, "y", &STATICS.type_f2, NULL);
        BUILTIN_2 (pow_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Pow,
                   "x", &STATICS.type_f3, NULL, "y", &STATICS.type_f3, NULL);
        BUILTIN_2 (pow_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Pow,
                   "x", &STATICS.type_f4, NULL, "y", &STATICS.type_f4, NULL);

        BUILTIN_1 (exp_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Exp,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (exp_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Exp,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (exp_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Exp,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (exp_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Exp,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (log_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Log,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (log_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Log,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (log_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Log,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (log_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Log,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (exp2_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Exp2,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (exp2_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Exp2,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (exp2_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Exp2,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (exp2_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Exp2,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (log2_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Log2,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (log2_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Log2,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (log2_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Log2,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (log2_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Log2,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (sqrt_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Sqrt,
                   "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (sqrt_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Sqrt,
                   "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (sqrt_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Sqrt,
                   "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (sqrt_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Sqrt,
                   "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (inverse_sqrt_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450InverseSqrt, "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (inverse_sqrt_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450InverseSqrt, "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (inverse_sqrt_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450InverseSqrt, "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (inverse_sqrt_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450InverseSqrt, "value", &STATICS.type_f4, NULL);

        BUILTIN_1 (determinant_f3x3, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Determinant, "matrix", NULL, &STATICS.type_f3x3);
        BUILTIN_1 (determinant_f4x4, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Determinant, "matrix", NULL, &STATICS.type_f4x4);

        BUILTIN_1 (inverse_matrix_f3x3, NULL, &STATICS.type_f3x3, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450MatrixInverse, "matrix", NULL, &STATICS.type_f3x3);
        BUILTIN_1 (inverse_matrix_f4x4, NULL, &STATICS.type_f4x4, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450MatrixInverse, "matrix", NULL, &STATICS.type_f4x4);

        BUILTIN_1 (transpose_matrix_f3x3, NULL, &STATICS.type_f3x3, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL,
                   SPIRV_INTERNAL, "matrix", NULL, &STATICS.type_f3x3);
        BUILTIN_1 (transpose_matrix_f4x4, NULL, &STATICS.type_f4x4, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL,
                   SPIRV_INTERNAL, "matrix", NULL, &STATICS.type_f4x4);

        BUILTIN_2 (min_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FMin,
                   "left", &STATICS.type_f1, NULL, "right", &STATICS.type_f1, NULL);
        BUILTIN_2 (min_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FMin,
                   "left", &STATICS.type_f2, NULL, "right", &STATICS.type_f2, NULL);
        BUILTIN_2 (min_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FMin,
                   "left", &STATICS.type_f3, NULL, "right", &STATICS.type_f3, NULL);
        BUILTIN_2 (min_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FMin,
                   "left", &STATICS.type_f4, NULL, "right", &STATICS.type_f4, NULL);

        BUILTIN_2 (min_i1, &STATICS.type_i1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SMin,
                   "left", &STATICS.type_i1, NULL, "right", &STATICS.type_i1, NULL);
        BUILTIN_2 (min_i2, &STATICS.type_i2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SMin,
                   "left", &STATICS.type_i2, NULL, "right", &STATICS.type_i2, NULL);
        BUILTIN_2 (min_i3, &STATICS.type_i3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SMin,
                   "left", &STATICS.type_i3, NULL, "right", &STATICS.type_i3, NULL);
        BUILTIN_2 (min_i4, &STATICS.type_i4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SMin,
                   "left", &STATICS.type_i4, NULL, "right", &STATICS.type_i4, NULL);

        BUILTIN_2 (max_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FMax,
                   "left", &STATICS.type_f1, NULL, "right", &STATICS.type_f1, NULL);
        BUILTIN_2 (max_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FMax,
                   "left", &STATICS.type_f2, NULL, "right", &STATICS.type_f2, NULL);
        BUILTIN_2 (max_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FMax,
                   "left", &STATICS.type_f3, NULL, "right", &STATICS.type_f3, NULL);
        BUILTIN_2 (max_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FMax,
                   "left", &STATICS.type_f4, NULL, "right", &STATICS.type_f4, NULL);

        BUILTIN_2 (max_i1, &STATICS.type_i1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SMax,
                   "left", &STATICS.type_i1, NULL, "right", &STATICS.type_i1, NULL);
        BUILTIN_2 (max_i2, &STATICS.type_i2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SMax,
                   "left", &STATICS.type_i2, NULL, "right", &STATICS.type_i2, NULL);
        BUILTIN_2 (max_i3, &STATICS.type_i3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SMax,
                   "left", &STATICS.type_i3, NULL, "right", &STATICS.type_i3, NULL);
        BUILTIN_2 (max_i4, &STATICS.type_i4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SMax,
                   "left", &STATICS.type_i4, NULL, "right", &STATICS.type_i4, NULL);

        BUILTIN_3 (clamp_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450FClamp, "value", &STATICS.type_f1, NULL, "min", &STATICS.type_f1, NULL, "max",
                   &STATICS.type_f1, NULL);
        BUILTIN_3 (clamp_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450FClamp, "value", &STATICS.type_f2, NULL, "min", &STATICS.type_f2, NULL, "max",
                   &STATICS.type_f2, NULL);
        BUILTIN_3 (clamp_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450FClamp, "value", &STATICS.type_f3, NULL, "min", &STATICS.type_f3, NULL, "max",
                   &STATICS.type_f3, NULL);
        BUILTIN_3 (clamp_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450FClamp, "value", &STATICS.type_f4, NULL, "min", &STATICS.type_f4, NULL, "max",
                   &STATICS.type_f4, NULL);

        BUILTIN_3 (clamp_i1, &STATICS.type_i1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450SClamp, "value", &STATICS.type_i1, NULL, "min", &STATICS.type_i1, NULL, "max",
                   &STATICS.type_i1, NULL);
        BUILTIN_3 (clamp_i2, &STATICS.type_i2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450SClamp, "value", &STATICS.type_i2, NULL, "min", &STATICS.type_i2, NULL, "max",
                   &STATICS.type_i2, NULL);
        BUILTIN_3 (clamp_i3, &STATICS.type_i3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450SClamp, "value", &STATICS.type_i3, NULL, "min", &STATICS.type_i3, NULL, "max",
                   &STATICS.type_i3, NULL);
        BUILTIN_3 (clamp_i4, &STATICS.type_i4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450SClamp, "value", &STATICS.type_i4, NULL, "min", &STATICS.type_i4, NULL, "max",
                   &STATICS.type_i4, NULL);

        BUILTIN_3 (mix_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FMix,
                   "left", &STATICS.type_f1, NULL, "right", &STATICS.type_f1, NULL, "alpha", &STATICS.type_f1, NULL);
        BUILTIN_3 (mix_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FMix,
                   "left", &STATICS.type_f2, NULL, "right", &STATICS.type_f2, NULL, "alpha", &STATICS.type_f2, NULL);
        BUILTIN_3 (mix_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FMix,
                   "left", &STATICS.type_f3, NULL, "right", &STATICS.type_f3, NULL, "alpha", &STATICS.type_f3, NULL);
        BUILTIN_3 (mix_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FMix,
                   "left", &STATICS.type_f4, NULL, "right", &STATICS.type_f4, NULL, "alpha", &STATICS.type_f4, NULL);

        BUILTIN_3 (fma_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Fma,
                   "left", &STATICS.type_f1, NULL, "right", &STATICS.type_f1, NULL, "addition", &STATICS.type_f1, NULL);
        BUILTIN_3 (fma_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Fma,
                   "left", &STATICS.type_f2, NULL, "right", &STATICS.type_f2, NULL, "addition", &STATICS.type_f2, NULL);
        BUILTIN_3 (fma_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Fma,
                   "left", &STATICS.type_f3, NULL, "right", &STATICS.type_f3, NULL, "addition", &STATICS.type_f3, NULL);
        BUILTIN_3 (fma_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Fma,
                   "left", &STATICS.type_f4, NULL, "right", &STATICS.type_f4, NULL, "addition", &STATICS.type_f4, NULL);

        BUILTIN_1 (length_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Length, "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (length_f2, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Length, "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (length_f3, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Length, "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (length_f4, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Length, "value", &STATICS.type_f4, NULL);

        BUILTIN_2 (distance_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Distance, "left", &STATICS.type_f1, NULL, "right", &STATICS.type_f1, NULL);
        BUILTIN_2 (distance_f2, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Distance, "left", &STATICS.type_f2, NULL, "right", &STATICS.type_f2, NULL);
        BUILTIN_2 (distance_f3, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Distance, "left", &STATICS.type_f3, NULL, "right", &STATICS.type_f3, NULL);
        BUILTIN_2 (distance_f4, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Distance, "left", &STATICS.type_f4, NULL, "right", &STATICS.type_f4, NULL);

        BUILTIN_2 (cross_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Cross,
                   "left", &STATICS.type_f3, NULL, "right", &STATICS.type_f3, NULL);

        BUILTIN_2 (dot_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL, "left",
                   &STATICS.type_f1, NULL, "right", &STATICS.type_f1, NULL);
        BUILTIN_2 (dot_f2, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL, "left",
                   &STATICS.type_f2, NULL, "right", &STATICS.type_f2, NULL);
        BUILTIN_2 (dot_f3, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL, "left",
                   &STATICS.type_f3, NULL, "right", &STATICS.type_f3, NULL);
        BUILTIN_2 (dot_f4, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL, "left",
                   &STATICS.type_f4, NULL, "right", &STATICS.type_f4, NULL);

        BUILTIN_1 (normalize_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Normalize, "value", &STATICS.type_f1, NULL);
        BUILTIN_1 (normalize_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Normalize, "value", &STATICS.type_f2, NULL);
        BUILTIN_1 (normalize_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Normalize, "value", &STATICS.type_f3, NULL);
        BUILTIN_1 (normalize_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Normalize, "value", &STATICS.type_f4, NULL);

        BUILTIN_2 (reflect_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Reflect, "incident", &STATICS.type_f1, NULL, "normal", &STATICS.type_f1, NULL);
        BUILTIN_2 (reflect_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Reflect, "incident", &STATICS.type_f2, NULL, "normal", &STATICS.type_f2, NULL);
        BUILTIN_2 (reflect_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Reflect, "incident", &STATICS.type_f3, NULL, "normal", &STATICS.type_f3, NULL);
        BUILTIN_2 (reflect_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Reflect, "incident", &STATICS.type_f4, NULL, "normal", &STATICS.type_f4, NULL);

        BUILTIN_3 (refract_f1, &STATICS.type_f1, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Refract, "incident", &STATICS.type_f1, NULL, "normal", &STATICS.type_f1, NULL,
                   "refraction", &STATICS.type_f1, NULL);
        BUILTIN_3 (refract_f2, &STATICS.type_f2, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Refract, "incident", &STATICS.type_f2, NULL, "normal", &STATICS.type_f2, NULL,
                   "refraction", &STATICS.type_f1, NULL);
        BUILTIN_3 (refract_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Refract, "incident", &STATICS.type_f3, NULL, "normal", &STATICS.type_f3, NULL,
                   "refraction", &STATICS.type_f1, NULL);
        BUILTIN_3 (refract_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY,
                   GLSLstd450Refract, "incident", &STATICS.type_f4, NULL, "normal", &STATICS.type_f4, NULL,
                   "refraction", &STATICS.type_f1, NULL);

        BUILTIN_2 (expand_f3_to_f4, &STATICS.type_f4, NULL, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL, "v3",
                   &STATICS.type_f3, NULL, "last_element", &STATICS.type_f1, NULL);

        BUILTIN_1 (crop_f4_to_f3, &STATICS.type_f3, NULL, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL, "v4",
                   &STATICS.type_f4, NULL);

        BUILTIN_1 (crop_f4x4_to_f3x3, NULL, &STATICS.type_f3x3, KAN_FALSE, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL,
                   "v4", NULL, &STATICS.type_f4x4);

#undef BUILTIN_COMMON
#undef BUILTIN_ARGUMENT
#undef BUILTIN_1
#undef BUILTIN_2
#undef BUILTIN_3

        statics_initialized = KAN_TRUE;
    }
}
