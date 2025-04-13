#include <spirv/unified1/GLSL.std.450.h>

#define KAN_RPL_COMPILER_IMPLEMENTATION
#include <kan/render_pipeline_language/compiler_internal.h>

static kan_bool_t statics_initialized = KAN_FALSE;
static struct kan_atomic_int_t statics_initialization_lock = {.value = 0};
struct kan_rpl_compiler_statics_t kan_rpl_compiler_statics;

void kan_rpl_compiler_ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        kan_atomic_int_lock (&statics_initialization_lock);
        if (statics_initialized)
        {
            kan_atomic_int_unlock (&statics_initialization_lock);
            return;
        }

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

        STATICS.interned_none = kan_string_intern ("none");
        STATICS.interned_back = kan_string_intern ("back");
        STATICS.interned_front = kan_string_intern ("front");

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
        STATICS.interned_sampler = kan_string_intern ("sampler");

        STATICS.interned_image_color_2d = kan_string_intern ("image_color_2d");
        STATICS.interned_image_color_3d = kan_string_intern ("image_color_3d");
        STATICS.interned_image_color_cube = kan_string_intern ("image_color_cube");
        STATICS.interned_image_color_2d_array = kan_string_intern ("image_color_2d_array");
        STATICS.interned_image_depth_2d = kan_string_intern ("image_depth_2d");
        STATICS.interned_image_depth_3d = kan_string_intern ("image_depth_3d");
        STATICS.interned_image_depth_cube = kan_string_intern ("image_depth_cube");
        STATICS.interned_image_depth_2d_array = kan_string_intern ("image_depth_2d_array");

        STATICS.interned_in = kan_string_intern ("in");
        STATICS.interned_out = kan_string_intern ("out");

        for (kan_loop_size_t vector_item_count = 0u; vector_item_count < INBUILT_VECTOR_MAX_ITEMS; ++vector_item_count)
        {
            STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_FLOAT, vector_item_count)] =
                (struct inbuilt_vector_type_t) {
                    .name = kan_string_intern ("f1"),
                    .item = INBUILT_TYPE_ITEM_FLOAT,
                    .items_count = 1u,
                    .meta_type = KAN_RPL_META_VARIABLE_TYPE_F1,
                };
        }

        STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_FLOAT, 1u)] = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("f1"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .items_count = 1u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F1,
        };

        STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_FLOAT, 2u)] = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("f2"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .items_count = 2u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F2,
        };

        STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_FLOAT, 3u)] = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("f3"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .items_count = 3u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F3,
        };

        STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_FLOAT, 4u)] = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("f4"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .items_count = 4u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F4,
        };

        STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_UNSIGNED, 1u)] =
            (struct inbuilt_vector_type_t) {
                .name = kan_string_intern ("u1"),
                .item = INBUILT_TYPE_ITEM_UNSIGNED,
                .items_count = 1u,
                .meta_type = KAN_RPL_META_VARIABLE_TYPE_U1,
            };

        STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_UNSIGNED, 2u)] =
            (struct inbuilt_vector_type_t) {
                .name = kan_string_intern ("u2"),
                .item = INBUILT_TYPE_ITEM_UNSIGNED,
                .items_count = 2u,
                .meta_type = KAN_RPL_META_VARIABLE_TYPE_U2,
            };

        STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_UNSIGNED, 3u)] =
            (struct inbuilt_vector_type_t) {
                .name = kan_string_intern ("u3"),
                .item = INBUILT_TYPE_ITEM_UNSIGNED,
                .items_count = 3u,
                .meta_type = KAN_RPL_META_VARIABLE_TYPE_U3,
            };

        STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_UNSIGNED, 4u)] =
            (struct inbuilt_vector_type_t) {
                .name = kan_string_intern ("u4"),
                .item = INBUILT_TYPE_ITEM_UNSIGNED,
                .items_count = 4u,
                .meta_type = KAN_RPL_META_VARIABLE_TYPE_U4,
            };

        STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_SIGNED, 1u)] =
            (struct inbuilt_vector_type_t) {
                .name = kan_string_intern ("s1"),
                .item = INBUILT_TYPE_ITEM_SIGNED,
                .items_count = 1u,
                .meta_type = KAN_RPL_META_VARIABLE_TYPE_S1,
            };

        STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_SIGNED, 2u)] =
            (struct inbuilt_vector_type_t) {
                .name = kan_string_intern ("s2"),
                .item = INBUILT_TYPE_ITEM_SIGNED,
                .items_count = 2u,
                .meta_type = KAN_RPL_META_VARIABLE_TYPE_S2,
            };

        STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_SIGNED, 3u)] =
            (struct inbuilt_vector_type_t) {
                .name = kan_string_intern ("s3"),
                .item = INBUILT_TYPE_ITEM_SIGNED,
                .items_count = 3u,
                .meta_type = KAN_RPL_META_VARIABLE_TYPE_S3,
            };

        STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_SIGNED, 4u)] =
            (struct inbuilt_vector_type_t) {
                .name = kan_string_intern ("s4"),
                .item = INBUILT_TYPE_ITEM_SIGNED,
                .items_count = 4u,
                .meta_type = KAN_RPL_META_VARIABLE_TYPE_S4,
            };

        struct inbuilt_matrix_type_t *type_pointer_f3x3 = &STATICS.matrix_types[0u];
        STATICS.matrix_types[0u] = (struct inbuilt_matrix_type_t) {
            .name = kan_string_intern ("f3x3"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .rows = 3u,
            .columns = 3u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F3X3,
        };

        struct inbuilt_matrix_type_t *type_pointer_f4x4 = &STATICS.matrix_types[1u];
        STATICS.matrix_types[1u] = (struct inbuilt_matrix_type_t) {
            .name = kan_string_intern ("f4x4"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .rows = 4u,
            .columns = 4u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F4X4,
        };

        struct compiler_instance_type_definition_t type_definition_in_void = {
            .class = COMPILER_INSTANCE_TYPE_CLASS_VOID,
            .access = KAN_RPL_ACCESS_CLASS_READ_ONLY,
            .array_size_runtime = KAN_FALSE,
            .array_dimensions_count = 0u,
            .array_dimensions = NULL,
        };

#define VECTOR_IN_TYPE_DEFINITION(TYPE, ITEM_TYPE, ITEMS_COUNT)                                                        \
    struct compiler_instance_type_definition_t type_definition_in_##TYPE = {                                           \
        .class = COMPILER_INSTANCE_TYPE_CLASS_VECTOR,                                                                  \
        .vector_data = &STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (ITEM_TYPE, ITEMS_COUNT)],                      \
        .access = KAN_RPL_ACCESS_CLASS_READ_ONLY,                                                                      \
        .array_size_runtime = KAN_FALSE,                                                                               \
        .array_dimensions_count = 0u,                                                                                  \
        .array_dimensions = NULL,                                                                                      \
    }

        VECTOR_IN_TYPE_DEFINITION (f1, INBUILT_TYPE_ITEM_FLOAT, 1u);
        VECTOR_IN_TYPE_DEFINITION (f2, INBUILT_TYPE_ITEM_FLOAT, 2u);
        VECTOR_IN_TYPE_DEFINITION (f3, INBUILT_TYPE_ITEM_FLOAT, 3u);
        VECTOR_IN_TYPE_DEFINITION (f4, INBUILT_TYPE_ITEM_FLOAT, 4u);
        VECTOR_IN_TYPE_DEFINITION (u1, INBUILT_TYPE_ITEM_UNSIGNED, 1u);
        VECTOR_IN_TYPE_DEFINITION (u2, INBUILT_TYPE_ITEM_UNSIGNED, 2u);
        VECTOR_IN_TYPE_DEFINITION (u3, INBUILT_TYPE_ITEM_UNSIGNED, 3u);
        VECTOR_IN_TYPE_DEFINITION (u4, INBUILT_TYPE_ITEM_UNSIGNED, 4u);
        VECTOR_IN_TYPE_DEFINITION (s1, INBUILT_TYPE_ITEM_SIGNED, 1u);
        VECTOR_IN_TYPE_DEFINITION (s2, INBUILT_TYPE_ITEM_SIGNED, 2u);
        VECTOR_IN_TYPE_DEFINITION (s3, INBUILT_TYPE_ITEM_SIGNED, 3u);
        VECTOR_IN_TYPE_DEFINITION (s4, INBUILT_TYPE_ITEM_SIGNED, 4u);

#define MATRIX_IN_TYPE_DEFINITION(TYPE)                                                                                \
    struct compiler_instance_type_definition_t type_definition_in_##TYPE = {                                           \
        .class = COMPILER_INSTANCE_TYPE_CLASS_MATRIX,                                                                  \
        .matrix_data = type_pointer_##TYPE,                                                                            \
        .access = KAN_RPL_ACCESS_CLASS_READ_ONLY,                                                                      \
        .array_size_runtime = KAN_FALSE,                                                                               \
        .array_dimensions_count = 0u,                                                                                  \
        .array_dimensions = NULL,                                                                                      \
    }

        MATRIX_IN_TYPE_DEFINITION (f3x3);
        MATRIX_IN_TYPE_DEFINITION (f4x4);

        const kan_interned_string_t interned_sampler = kan_string_intern ("sampler");
        const kan_interned_string_t interned_calls = kan_string_intern ("calls");
        STATICS.sample_function_name = kan_string_intern ("sample");
        STATICS.sample_dref_function_name = kan_string_intern ("sample_dref");

#define SAMPLER_ARGUMENT(TYPE, NAME, NEXT)                                                                             \
    (struct compiler_instance_function_argument_node_t) {                                                              \
        .next = NEXT,                                                                                                  \
        .variable =                                                                                                    \
            {                                                                                                          \
                .name = kan_string_intern (#NAME),                                                                     \
                .type = type_definition_in_##TYPE,                                                                     \
            },                                                                                                         \
        .module_name = interned_sampler,                                                                               \
        .source_name = interned_calls,                                                                                 \
        .source_line = 0u,                                                                                             \
    }

        STATICS.sample_2d_additional_arguments[0u] = SAMPLER_ARGUMENT (f2, coordinates, NULL);
        STATICS.sample_3d_additional_arguments[0u] = SAMPLER_ARGUMENT (f3, coordinates, NULL);
        STATICS.sample_cube_additional_arguments[0u] = SAMPLER_ARGUMENT (f3, coordinates, NULL);

        STATICS.sample_2d_array_additional_arguments[0u] =
            SAMPLER_ARGUMENT (u1, layer, &STATICS.sample_2d_array_additional_arguments[1u]);
        STATICS.sample_2d_array_additional_arguments[1u] = SAMPLER_ARGUMENT (f2, coordinates, NULL);

        STATICS.sample_dref_2d_additional_arguments[0u] =
            SAMPLER_ARGUMENT (f2, coordinates, &STATICS.sample_dref_2d_additional_arguments[1u]);
        STATICS.sample_dref_2d_additional_arguments[1u] = SAMPLER_ARGUMENT (f1, reference, NULL);

        STATICS.sample_dref_3d_additional_arguments[0u] =
            SAMPLER_ARGUMENT (f3, coordinates, &STATICS.sample_dref_3d_additional_arguments[1u]);
        STATICS.sample_dref_3d_additional_arguments[1u] = SAMPLER_ARGUMENT (f1, reference, NULL);

        STATICS.sample_dref_cube_additional_arguments[0u] =
            SAMPLER_ARGUMENT (f3, coordinates, &STATICS.sample_dref_cube_additional_arguments[1u]);
        STATICS.sample_dref_cube_additional_arguments[1u] = SAMPLER_ARGUMENT (f1, reference, NULL);

        STATICS.sample_dref_2d_array_additional_arguments[0u] =
            SAMPLER_ARGUMENT (u1, array_layer, &STATICS.sample_dref_2d_array_additional_arguments[1u]);
        STATICS.sample_dref_2d_array_additional_arguments[1u] =
            SAMPLER_ARGUMENT (f2, coordinates, &STATICS.sample_dref_2d_array_additional_arguments[2u]);
        STATICS.sample_dref_2d_array_additional_arguments[2u] = SAMPLER_ARGUMENT (f1, reference, NULL);

#define ANY_STAGE KAN_INT_MAX (int32_t)
#define SPIRV_INTERNAL ((spirv_size_t) SPIRV_FIXED_ID_INVALID)

#define BUILTIN_COMMON(NAME, RETURN_TYPE, REQUIRED_STAGE, SPIRV_EXTERNAL_LIBRARY, SPIRV_EXTERNAL_INSTRUCTION,          \
                       ARGUMENTS)                                                                                      \
    STATICS.builtin_##NAME = (struct compiler_instance_function_node_t) {                                              \
        .next = NULL,                                                                                                  \
        .name = kan_string_intern (#NAME),                                                                             \
        .return_type = type_definition_in_##RETURN_TYPE,                                                               \
        .first_argument = ARGUMENTS,                                                                                   \
        .body = NULL,                                                                                                  \
        .has_stage_specific_access = REQUIRED_STAGE == ANY_STAGE ? KAN_FALSE : KAN_TRUE,                               \
        .required_stage = REQUIRED_STAGE,                                                                              \
        .first_buffer_access = NULL,                                                                                   \
        .first_sampler_access = NULL,                                                                                  \
        .spirv_external_library_id = (spirv_size_t) SPIRV_EXTERNAL_LIBRARY,                                            \
        .spirv_external_instruction_id = (spirv_size_t) SPIRV_EXTERNAL_INSTRUCTION,                                    \
        .module_name = module_standard,                                                                                \
        .source_name = source_functions,                                                                               \
        .source_line = 0u,                                                                                             \
    };                                                                                                                 \
                                                                                                                       \
    struct kan_rpl_compiler_builtin_node_t *node_##NAME = kan_allocate_batched (                                       \
        STATICS.rpl_compiler_builtin_hash_allocation_group, sizeof (struct kan_rpl_compiler_builtin_node_t));          \
    node_##NAME->node.hash = KAN_HASH_OBJECT_POINTER (STATICS.builtin_##NAME.name);                                    \
    node_##NAME->name = STATICS.builtin_##NAME.name;                                                                   \
    node_##NAME->builtin = &STATICS.builtin_##NAME;                                                                    \
    kan_hash_storage_add (&STATICS.builtin_hash_storage, &node_##NAME->node)

#define BUILTIN_ARGUMENT(BULTIN, INDEX, NEXT, TYPE, NAME)                                                              \
    STATICS.builtin_##BULTIN##_arguments[INDEX] = (struct compiler_instance_function_argument_node_t) {                \
        .next = NEXT,                                                                                                  \
        .variable =                                                                                                    \
            {                                                                                                          \
                .name = kan_string_intern (#NAME),                                                                     \
                .type = type_definition_in_##TYPE,                                                                     \
            },                                                                                                         \
        .module_name = module_standard,                                                                                \
        .source_name = source_functions,                                                                               \
        .source_line = 0u,                                                                                             \
    }

#define BUILTIN_0(NAME, RETURN_TYPE, REQUIRED_STAGE, SPIRV_EXTERNAL_LIBRARY, SPIRV_EXTERNAL_INSTRUCTION)               \
    BUILTIN_COMMON (NAME, RETURN_TYPE, REQUIRED_STAGE, SPIRV_EXTERNAL_LIBRARY, SPIRV_EXTERNAL_INSTRUCTION, NULL)

#define BUILTIN_1(NAME, RETURN_TYPE, REQUIRED_STAGE, SPIRV_EXTERNAL_LIBRARY, SPIRV_EXTERNAL_INSTRUCTION,               \
                  ARGUMENT_1_TYPE, ARGUMENT_1_NAME)                                                                    \
    BUILTIN_ARGUMENT (NAME, 0u, NULL, ARGUMENT_1_TYPE, ARGUMENT_1_NAME);                                               \
    BUILTIN_COMMON (NAME, RETURN_TYPE, REQUIRED_STAGE, SPIRV_EXTERNAL_LIBRARY, SPIRV_EXTERNAL_INSTRUCTION,             \
                    STATICS.builtin_##NAME##_arguments)

#define BUILTIN_2(NAME, RETURN_TYPE, REQUIRED_STAGE, SPIRV_EXTERNAL_LIBRARY, SPIRV_EXTERNAL_INSTRUCTION,               \
                  ARGUMENT_1_TYPE, ARGUMENT_1_NAME, ARGUMENT_2_TYPE, ARGUMENT_2_NAME)                                  \
    BUILTIN_ARGUMENT (NAME, 0u, &STATICS.builtin_##NAME##_arguments[1u], ARGUMENT_1_TYPE, ARGUMENT_1_NAME);            \
    BUILTIN_ARGUMENT (NAME, 1u, NULL, ARGUMENT_2_TYPE, ARGUMENT_2_NAME);                                               \
    BUILTIN_COMMON (NAME, RETURN_TYPE, REQUIRED_STAGE, SPIRV_EXTERNAL_LIBRARY, SPIRV_EXTERNAL_INSTRUCTION,             \
                    STATICS.builtin_##NAME##_arguments)

#define BUILTIN_3(NAME, RETURN_TYPE, REQUIRED_STAGE, SPIRV_EXTERNAL_LIBRARY, SPIRV_EXTERNAL_INSTRUCTION,               \
                  ARGUMENT_1_TYPE, ARGUMENT_1_NAME, ARGUMENT_2_TYPE, ARGUMENT_2_NAME, ARGUMENT_3_TYPE,                 \
                  ARGUMENT_3_NAME)                                                                                     \
    BUILTIN_ARGUMENT (NAME, 0u, &STATICS.builtin_##NAME##_arguments[1u], ARGUMENT_1_TYPE, ARGUMENT_1_NAME);            \
    BUILTIN_ARGUMENT (NAME, 1u, &STATICS.builtin_##NAME##_arguments[2u], ARGUMENT_2_TYPE, ARGUMENT_2_NAME);            \
    BUILTIN_ARGUMENT (NAME, 2u, NULL, ARGUMENT_3_TYPE, ARGUMENT_3_NAME);                                               \
    BUILTIN_COMMON (NAME, RETURN_TYPE, REQUIRED_STAGE, SPIRV_EXTERNAL_LIBRARY, SPIRV_EXTERNAL_INSTRUCTION,             \
                    STATICS.builtin_##NAME##_arguments)

        const kan_interned_string_t module_standard = kan_string_intern ("standard");
        const kan_interned_string_t source_functions = kan_string_intern ("functions");

        kan_hash_storage_init (&STATICS.builtin_hash_storage, STATICS.rpl_compiler_builtin_hash_allocation_group,
                               KAN_RPL_BUILTIN_HASH_STORAGE_BUCKETS);

        BUILTIN_1 (vertex_stage_output_position, void, KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX, SPIRV_INTERNAL,
                   SPIRV_INTERNAL, f4, position);

        BUILTIN_0 (fragment_stage_discard, void, KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT, SPIRV_INTERNAL,
                   SPIRV_INTERNAL);

        BUILTIN_0 (pi, f1, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL);

#define ANY_FLOAT_VECTOR_BUILTINS(TYPE)                                                                                \
    BUILTIN_1 (round_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Round, TYPE, value);              \
    BUILTIN_1 (round_even_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450RoundEven, TYPE, value);     \
    BUILTIN_1 (trunc_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Trunc, TYPE, value);              \
    BUILTIN_1 (abs_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FAbs, TYPE, value);                 \
    BUILTIN_1 (sign_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FSign, TYPE, value);               \
    BUILTIN_1 (floor_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Floor, TYPE, value);              \
    BUILTIN_1 (ceil_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Ceil, TYPE, value);                \
    BUILTIN_1 (fract_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Fract, TYPE, value);              \
    BUILTIN_1 (sin_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Sin, TYPE, value);                  \
    BUILTIN_1 (cos_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Cos, TYPE, value);                  \
    BUILTIN_1 (tan_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Tan, TYPE, value);                  \
    BUILTIN_1 (asin_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Asin, TYPE, value);                \
    BUILTIN_1 (acos_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Acos, TYPE, value);                \
    BUILTIN_1 (atan_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Atan, TYPE, value);                \
    BUILTIN_1 (sinh_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Sinh, TYPE, value);                \
    BUILTIN_1 (cosh_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Cosh, TYPE, value);                \
    BUILTIN_1 (tanh_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Tanh, TYPE, value);                \
    BUILTIN_1 (asinh_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Asinh, TYPE, value);              \
    BUILTIN_1 (acosh_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Acosh, TYPE, value);              \
    BUILTIN_1 (atanh_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Atanh, TYPE, value);              \
    BUILTIN_1 (atan2_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Atan2, TYPE, value);              \
    BUILTIN_2 (pow_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Pow, TYPE, x, TYPE, y);             \
    BUILTIN_1 (exp_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Exp, TYPE, value);                  \
    BUILTIN_1 (log_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Log, TYPE, value);                  \
    BUILTIN_1 (exp2_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Exp2, TYPE, value);                \
    BUILTIN_1 (log2_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Log2, TYPE, value);                \
    BUILTIN_1 (sqrt_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Sqrt, TYPE, value);                \
    BUILTIN_1 (inverse_sqrt_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450InverseSqrt, TYPE, value); \
    BUILTIN_2 (min_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FMin, TYPE, x, TYPE, y);            \
    BUILTIN_2 (max_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FMax, TYPE, x, TYPE, y);            \
    BUILTIN_3 (clamp_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FClamp, TYPE, value, TYPE, min,   \
               TYPE, max);                                                                                             \
    BUILTIN_3 (mix_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450FMix, TYPE, x, TYPE, y, TYPE,       \
               alpha);                                                                                                 \
    BUILTIN_2 (step_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Step, TYPE, edge, TYPE, value);    \
    BUILTIN_3 (fma_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Fma, TYPE, x, TYPE, y, TYPE, a);    \
    BUILTIN_2 (reflect_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Reflect, TYPE, incident, TYPE,  \
               normal);                                                                                                \
    BUILTIN_3 (refract_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Refract, TYPE, incident, TYPE,  \
               normal, TYPE, refraction)

        ANY_FLOAT_VECTOR_BUILTINS (f1);
        ANY_FLOAT_VECTOR_BUILTINS (f2);
        ANY_FLOAT_VECTOR_BUILTINS (f3);
        ANY_FLOAT_VECTOR_BUILTINS (f4);

#define ANY_MULTI_ITEM_FLOAT_VECTOR_BUILTINS(TYPE)                                                                     \
    BUILTIN_1 (length_##TYPE, f1, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Length, TYPE, value);              \
    BUILTIN_2 (distance_##TYPE, f1, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Distance, TYPE, x, TYPE, y);     \
    BUILTIN_2 (dot_##TYPE, f1, ANY_STAGE, SPIRV_INTERNAL, SPIRV_INTERNAL, TYPE, x, TYPE, y);                           \
    BUILTIN_1 (normalize_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Normalize, TYPE, value)

        ANY_MULTI_ITEM_FLOAT_VECTOR_BUILTINS (f2);
        ANY_MULTI_ITEM_FLOAT_VECTOR_BUILTINS (f3);
        ANY_MULTI_ITEM_FLOAT_VECTOR_BUILTINS (f4);

#define ANY_UNSIGNED_VECTOR_BUILTINS(TYPE)                                                                             \
    BUILTIN_2 (min_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SMin, TYPE, x, TYPE, y);            \
    BUILTIN_2 (max_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SMax, TYPE, x, TYPE, y);            \
    BUILTIN_3 (clamp_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SClamp, TYPE, value, TYPE, min,   \
               TYPE, max)

        ANY_UNSIGNED_VECTOR_BUILTINS (u1);
        ANY_UNSIGNED_VECTOR_BUILTINS (u2);
        ANY_UNSIGNED_VECTOR_BUILTINS (u3);
        ANY_UNSIGNED_VECTOR_BUILTINS (u4);

#define ANY_SIGNED_VECTOR_BUILTINS(TYPE)                                                                               \
    BUILTIN_1 (abs_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SAbs, TYPE, value);                 \
    BUILTIN_1 (sign_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SSign, TYPE, value);               \
    BUILTIN_2 (min_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SMin, TYPE, x, TYPE, y);            \
    BUILTIN_2 (max_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SMax, TYPE, x, TYPE, y);            \
    BUILTIN_3 (clamp_##TYPE, TYPE, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450SClamp, TYPE, value, TYPE, min,   \
               TYPE, max)

        ANY_SIGNED_VECTOR_BUILTINS (s1);
        ANY_SIGNED_VECTOR_BUILTINS (s2);
        ANY_SIGNED_VECTOR_BUILTINS (s3);
        ANY_SIGNED_VECTOR_BUILTINS (s4);

        BUILTIN_1 (determinant_f3x3, f1, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Determinant, f3x3, matrix);
        BUILTIN_1 (determinant_f4x4, f1, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Determinant, f4x4, matrix);

        BUILTIN_1 (inverse_f3x3, f1, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450MatrixInverse, f3x3, matrix);
        BUILTIN_1 (inverse_f4x4, f1, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450MatrixInverse, f4x4, matrix);

        BUILTIN_2 (cross_f3, f3, ANY_STAGE, SPIRV_FIXED_ID_GLSL_LIBRARY, GLSLstd450Cross, f3, x, f3, y);

        statics_initialized = KAN_TRUE;
        kan_atomic_int_unlock (&statics_initialization_lock);
    }
}
