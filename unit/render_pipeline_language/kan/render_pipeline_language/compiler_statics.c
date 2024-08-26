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
        STATICS.rpl_compiler_context_allocation_group =
            kan_allocation_group_get_child (STATICS.rpl_compiler_allocation_group, "context");
        STATICS.rpl_compiler_instance_allocation_group =
            kan_allocation_group_get_child (STATICS.rpl_compiler_allocation_group, "instance");

        STATICS.interned_fill = kan_string_intern ("fill");
        STATICS.interned_wireframe = kan_string_intern ("wireframe");
        STATICS.interned_back = kan_string_intern ("back");

        STATICS.interned_polygon_mode = kan_string_intern ("polygon_mode");
        STATICS.interned_cull_mode = kan_string_intern ("cull_mode");
        STATICS.interned_depth_test = kan_string_intern ("depth_test");
        STATICS.interned_depth_write = kan_string_intern ("depth_write");

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

        STATICS.glsl_450_builtin_functions_first = &STATICS.glsl_450_sqrt;
        const kan_interned_string_t module_glsl_450 = kan_string_intern ("glsl_450_standard");
        const kan_interned_string_t source_functions = kan_string_intern ("functions");

        STATICS.glsl_450_sqrt_arguments[0u] = (struct compiler_instance_declaration_node_t) {
            .next = NULL,
            .variable = {.name = kan_string_intern ("number"),
                         .type =
                             {
                                 .if_vector = &STATICS.type_f1,
                                 .if_matrix = NULL,
                                 .if_struct = NULL,
                                 .array_dimensions_count = 0u,
                                 .array_dimensions = NULL,
                             }},
            .meta_count = 0u,
            .meta = NULL,
            .module_name = module_glsl_450,
            .source_name = source_functions,
            .source_line = 0u,
        };

        STATICS.glsl_450_sqrt = (struct compiler_instance_function_node_t) {
            .next = NULL,
            .name = kan_string_intern ("sqrt"),
            .return_type_if_vector = &STATICS.type_f1,
            .return_type_if_matrix = NULL,
            .return_type_if_struct = NULL,
            .first_argument = STATICS.glsl_450_sqrt_arguments,
            .body = NULL,
            .has_stage_specific_access = KAN_FALSE,
            .required_stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX,
            .first_buffer_access = NULL,
            .first_sampler_access = NULL,
            .module_name = module_glsl_450,
            .source_name = source_functions,
            .source_line = 0u,
        };

        STATICS.shader_standard_builtin_functions_first = &STATICS.shader_standard_vertex_stage_output_position;
        const kan_interned_string_t module_shader_standard = kan_string_intern ("shader_standard");

        STATICS.shader_standard_vertex_stage_output_position_arguments[0u] =
            (struct compiler_instance_declaration_node_t) {
                .next = NULL,
                .variable = {.name = kan_string_intern ("position"),
                             .type =
                                 {
                                     .if_vector = &STATICS.type_f4,
                                     .if_matrix = NULL,
                                     .if_struct = NULL,
                                     .array_dimensions_count = 0u,
                                     .array_dimensions = NULL,
                                 }},
                .meta_count = 0u,
                .meta = NULL,
                .module_name = module_shader_standard,
                .source_name = source_functions,
                .source_line = 0u,
            };

        STATICS.shader_standard_vertex_stage_output_position = (struct compiler_instance_function_node_t) {
            .next = &STATICS.shader_standard_i1_to_f1,
            .name = kan_string_intern ("vertex_stage_output_position"),
            .return_type_if_vector = NULL,
            .return_type_if_matrix = NULL,
            .return_type_if_struct = NULL,
            .first_argument = STATICS.shader_standard_vertex_stage_output_position_arguments,
            .body = NULL,
            .has_stage_specific_access = KAN_TRUE,
            .required_stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX,
            .first_buffer_access = NULL,
            .first_sampler_access = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        STATICS.shader_standard_i1_to_f1_arguments[0u] = (struct compiler_instance_declaration_node_t) {
            .next = NULL,
            .variable = {.name = kan_string_intern ("value"),
                         .type =
                             {
                                 .if_vector = &STATICS.type_i1,
                                 .if_matrix = NULL,
                                 .if_struct = NULL,
                                 .array_dimensions_count = 0u,
                                 .array_dimensions = NULL,
                             }},
            .meta_count = 0u,
            .meta = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        STATICS.shader_standard_i1_to_f1 = (struct compiler_instance_function_node_t) {
            .next = &STATICS.shader_standard_i2_to_f2,
            .name = kan_string_intern ("i1_to_f1"),
            .return_type_if_vector = NULL,
            .return_type_if_matrix = NULL,
            .return_type_if_struct = NULL,
            .first_argument = STATICS.shader_standard_i1_to_f1_arguments,
            .body = NULL,
            .has_stage_specific_access = KAN_TRUE,
            .required_stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX,
            .first_buffer_access = NULL,
            .first_sampler_access = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        STATICS.shader_standard_i2_to_f2_arguments[0u] = (struct compiler_instance_declaration_node_t) {
            .next = NULL,
            .variable = {.name = kan_string_intern ("value"),
                         .type =
                             {
                                 .if_vector = &STATICS.type_i2,
                                 .if_matrix = NULL,
                                 .if_struct = NULL,
                                 .array_dimensions_count = 0u,
                                 .array_dimensions = NULL,
                             }},
            .meta_count = 0u,
            .meta = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        STATICS.shader_standard_i2_to_f2 = (struct compiler_instance_function_node_t) {
            .next = &STATICS.shader_standard_i3_to_f3,
            .name = kan_string_intern ("i2_to_f2"),
            .return_type_if_vector = NULL,
            .return_type_if_matrix = NULL,
            .return_type_if_struct = NULL,
            .first_argument = STATICS.shader_standard_i2_to_f2_arguments,
            .body = NULL,
            .has_stage_specific_access = KAN_TRUE,
            .required_stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX,
            .first_buffer_access = NULL,
            .first_sampler_access = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        STATICS.shader_standard_i3_to_f3_arguments[0u] = (struct compiler_instance_declaration_node_t) {
            .next = NULL,
            .variable = {.name = kan_string_intern ("value"),
                         .type =
                             {
                                 .if_vector = &STATICS.type_i3,
                                 .if_matrix = NULL,
                                 .if_struct = NULL,
                                 .array_dimensions_count = 0u,
                                 .array_dimensions = NULL,
                             }},
            .meta_count = 0u,
            .meta = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        STATICS.shader_standard_i3_to_f3 = (struct compiler_instance_function_node_t) {
            .next = &STATICS.shader_standard_i4_to_f4,
            .name = kan_string_intern ("i3_to_f3"),
            .return_type_if_vector = NULL,
            .return_type_if_matrix = NULL,
            .return_type_if_struct = NULL,
            .first_argument = STATICS.shader_standard_i3_to_f3_arguments,
            .body = NULL,
            .has_stage_specific_access = KAN_TRUE,
            .required_stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX,
            .first_buffer_access = NULL,
            .first_sampler_access = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        STATICS.shader_standard_i4_to_f4_arguments[0u] = (struct compiler_instance_declaration_node_t) {
            .next = NULL,
            .variable = {.name = kan_string_intern ("value"),
                         .type =
                             {
                                 .if_vector = &STATICS.type_i4,
                                 .if_matrix = NULL,
                                 .if_struct = NULL,
                                 .array_dimensions_count = 0u,
                                 .array_dimensions = NULL,
                             }},
            .meta_count = 0u,
            .meta = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        STATICS.shader_standard_i4_to_f4 = (struct compiler_instance_function_node_t) {
            .next = NULL,
            .name = kan_string_intern ("i4_to_f4"),
            .return_type_if_vector = NULL,
            .return_type_if_matrix = NULL,
            .return_type_if_struct = NULL,
            .first_argument = STATICS.shader_standard_i4_to_f4_arguments,
            .body = NULL,
            .has_stage_specific_access = KAN_TRUE,
            .required_stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX,
            .first_buffer_access = NULL,
            .first_sampler_access = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        statics_initialized = KAN_TRUE;
    }
}
