#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kan/context/render_backend_system.h>
#include <kan/inline_math/inline_math.h>
#include <kan/platform/application.h>
#include <kan/platform/precise_time.h>
#include <kan/render_backend_tools/render_backend_tools.h>
#include <kan/testing/testing.h>

struct render_image_config_t
{
    float bricks_count_horizontal;
    float bricks_count_vertical;
    float border_size_horizontal;
    float border_size_vertical;
    float brick_color_r;
    float brick_color_g;
    float brick_color_b;
    float brick_color_a;
    float border_color_r;
    float border_color_g;
    float border_color_b;
    float border_color_a;
    float image_border_r;
    float image_border_g;
    float image_border_b;
    float image_border_a;
    float image_border_size;
};

static const char *render_image_shader =
    "vertex_attribute_buffer vertex\n"
    "{\n"
    "    f2 position;\n"
    "};\n"
    "\n"
    "vertex_stage_output vertex_output\n"
    "{\n"
    "    f2 uv;\n"
    "};\n"
    "\n"
    "void vertex_main (void)\n"
    "{\n"
    "    vertex_output.uv = f2 {0.5, 0.5} + f2 {0.5, 0.5} * vertex.position;"
    "    vertex_stage_output_position (f4 {vertex.position._0, vertex.position._1, 0.0, 1.0});"
    "}\n"
    "\n"
    "read_only_storage_buffer config\n"
    "{\n"
    "    f2 bricks;\n"
    "    f2 border;"
    "    f4 brick_color;\n"
    "    f4 border_color;\n"
    "    f4 image_border_color;\n"
    "    f1 image_border_size;\n"
    "};\n"
    "\n"
    "fragment_stage_output fragment_output\n"
    "{\n"
    "    f4 color;\n"
    "};\n"
    "\n"
    "void fragment_main (void)\n"
    "{\n"
    "    f2 coordinates = vertex_output.uv * config.bricks;\n"
    "    if (f1_to_i1 (trunc_f1 (coordinates._1)) % 2 == 1)\n"
    "    {\n"
    "        coordinates._0 = coordinates._0 + 0.5;\n"
    "    }\n"
    "    \n"
    "    f2 local_coordinates = fract_f2 (coordinates);\n"
    "    f2 inverse_border = f2 {1.0, 1.0} - config.border;"
    "\n"
    "    f2 border_mask = min_f2 (\n"
    "        step_f2 (config.border, local_coordinates),\n"
    "        f2 {1.0, 1.0} - step_f2 (inverse_border, local_coordinates));\n"
    "    f1 is_brick = min_f1 (border_mask._0, border_mask._1);\n"
    "\n"
    "    f4 fragment_color = mix_f4 (\n"
    "        config.border_color, config.brick_color, f4 {is_brick, is_brick, is_brick, is_brick});\n"
    "\n"
    "    f2 image_border = f2 {config.image_border_size, config.image_border_size};\n"
    "    f2 inverse_image_border = f2 {1.0, 1.0} - image_border;\n"
    "    f2 image_border_mask = min_f2 (\n"
    "        step_f2 (image_border, vertex_output.uv),\n"
    "        f2 {1.0, 1.0} - step_f2 (inverse_image_border, vertex_output.uv));\n"
    "    f1 is_image_content = min_f1 (image_border_mask._0, image_border_mask._1);\n"
    "\n"
    "    fragment_output.color = mix_f4 (\n"
    "        config.image_border_color, fragment_color ,\n"
    "        f4 {is_image_content, is_image_content, is_image_content, is_image_content});\n"
    "}\n";

static kan_render_pass_t create_render_image_pass (kan_render_context_t render_context)
{
    struct kan_render_pass_attachment_t attachments[] = {
        {
            .type = KAN_RENDER_PASS_ATTACHMENT_COLOR,
            .color_format = KAN_RENDER_COLOR_FORMAT_RGBA32_SRGB,
            .samples = 1u,
            .load_operation = KAN_RENDER_LOAD_OPERATION_CLEAR,
            .store_operation = KAN_RENDER_STORE_OPERATION_STORE,
        },
    };

    struct kan_render_pass_description_t description = {
        .type = KAN_RENDER_PASS_GRAPHICS,
        .attachments_count = sizeof (attachments) / sizeof (attachments[0u]),
        .attachments = attachments,
        .tracking_name = kan_string_intern ("render_image_pass"),
    };

    return kan_render_pass_create (render_context, &description);
}

static kan_render_graphics_pipeline_t create_render_image_pipeline (kan_render_context_t render_context,
                                                                    kan_render_pass_t render_image_pass,
                                                                    uint64_t *output_attribute_binding,
                                                                    uint64_t *output_config_set,
                                                                    uint64_t *output_config_binding)
{
    kan_rpl_parser_t parser = kan_rpl_parser_create (kan_string_intern ("render_image"));
    KAN_TEST_ASSERT (kan_rpl_parser_add_source (parser, render_image_shader, kan_string_intern ("render_image")))
    struct kan_rpl_intermediate_t intermediate;
    kan_rpl_intermediate_init (&intermediate);
    KAN_TEST_ASSERT (kan_rpl_parser_build_intermediate (parser, &intermediate))
    kan_rpl_parser_destroy (parser);

    kan_rpl_compiler_context_t compiler_context =
        kan_rpl_compiler_context_create (KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC, kan_string_intern ("render_image"));
    kan_rpl_compiler_context_use_module (compiler_context, &intermediate);

    struct kan_dynamic_array_t code;
    struct kan_rpl_meta_t meta;
    kan_rpl_meta_init (&meta);

    struct kan_rpl_entry_point_t entry_points[] = {
        {
            .stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX,
            .function_name = kan_string_intern ("vertex_main"),
        },
        {
            .stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT,
            .function_name = kan_string_intern ("fragment_main"),
        },
    };

    kan_rpl_compiler_instance_t compiler_instance = kan_rpl_compiler_context_resolve (
        compiler_context, sizeof (entry_points) / sizeof (entry_points[0u]), entry_points);
    KAN_TEST_ASSERT (compiler_instance != KAN_INVALID_RPL_COMPILER_INSTANCE)

    KAN_TEST_ASSERT (
        kan_render_backend_tools_emit_platform_code (compiler_instance, &code, KAN_ALLOCATION_GROUP_IGNORE))
    KAN_TEST_ASSERT (kan_rpl_compiler_instance_emit_meta (compiler_instance, &meta))
    kan_rpl_compiler_instance_destroy (compiler_instance);

    kan_rpl_compiler_context_destroy (compiler_context);
    kan_rpl_intermediate_shutdown (&intermediate);

    struct kan_render_attribute_source_description_t attribute_sources[1u];
    struct kan_render_attribute_description_t attributes[1u];
    struct kan_render_parameter_binding_description_t parameter_set_bindings[1u];
    struct kan_render_parameter_set_description_t parameter_sets[1u];

    for (uint64_t buffer_index = 0u; buffer_index < meta.buffers.size; ++buffer_index)
    {
        struct kan_rpl_meta_buffer_t *buffer = &((struct kan_rpl_meta_buffer_t *) meta.buffers.data)[buffer_index];
        if (buffer->name == kan_string_intern ("vertex"))
        {
            attribute_sources[0u].binding = buffer->binding;
            *output_attribute_binding = buffer->binding;
            attribute_sources[0u].stride = buffer->size;
            attribute_sources[0u].rate = KAN_RENDER_ATTRIBUTE_RATE_PER_VERTEX;

            KAN_TEST_ASSERT (buffer->attributes.size == 1u)
            struct kan_rpl_meta_attribute_t *attribute =
                &((struct kan_rpl_meta_attribute_t *) buffer->attributes.data)[0u];

            attributes[0u].binding = buffer->binding;
            attributes[0u].location = attribute->location;
            attributes[0u].offset = attribute->offset;
            KAN_TEST_CHECK (attribute->type == KAN_RPL_META_VARIABLE_TYPE_F2)
            attributes[0u].format = KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_2;
        }
        else if (buffer->name == kan_string_intern ("config"))
        {
            parameter_sets[0u].set = buffer->set;
            *output_config_set = buffer->set;
            parameter_sets[0u].bindings_count = sizeof (parameter_set_bindings) / sizeof (parameter_set_bindings[0u]);
            parameter_sets[0u].bindings = parameter_set_bindings;
            parameter_sets[0u].stable_binding = KAN_TRUE;

            KAN_TEST_CHECK (buffer->type == KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE)
            parameter_set_bindings[0u].binding = buffer->binding;
            *output_config_binding = buffer->binding;
            parameter_set_bindings[0u].type = KAN_RENDER_PARAMETER_BINDING_TYPE_STORAGE_BUFFER;
            parameter_set_bindings[0u].used_stage_mask =
                (1u << KAN_RENDER_STAGE_GRAPHICS_VERTEX) | (1u << KAN_RENDER_STAGE_GRAPHICS_FRAGMENT);
        }
        else
        {
            KAN_TEST_CHECK (KAN_FALSE)
        }
    }

    struct kan_render_graphics_pipeline_family_description_t pipeline_family_description = {
        .topology = KAN_RENDER_GRAPHICS_TOPOLOGY_TRIANGLE_LIST,
        .attribute_sources_count = sizeof (attribute_sources) / (sizeof (attribute_sources[0u])),
        .attribute_sources = attribute_sources,
        .attributes_count = sizeof (attributes) / (sizeof (attributes[0u])),
        .attributes = attributes,
        .parameter_sets_count = sizeof (parameter_sets) / sizeof (parameter_sets[0u]),
        .parameter_sets = parameter_sets,
        .tracking_name = "render_image_pipeline_family",
    };

    kan_render_graphics_pipeline_family_t family =
        kan_render_graphics_pipeline_family_create (render_context, &pipeline_family_description);
    KAN_TEST_ASSERT (family != KAN_INVALID_RENDER_GRAPHICS_PIPELINE_FAMILY)

    kan_render_code_module_t code_module = kan_render_code_module_create (
        render_context, code.size * code.item_size, code.data, kan_string_intern ("render_image_code"));

    struct kan_render_color_output_setup_description_t output_setups[1u] = {
        {
            .use_blend = KAN_FALSE,
            .write_r = KAN_TRUE,
            .write_g = KAN_TRUE,
            .write_b = KAN_TRUE,
            .write_a = KAN_TRUE,
            .source_color_blend_factor = KAN_RENDER_BLEND_FACTOR_ZERO,
            .destination_color_blend_factor = KAN_RENDER_BLEND_FACTOR_ZERO,
            .color_blend_operation = KAN_RENDER_BLEND_OPERATION_MAX,
            .source_alpha_blend_factor = KAN_RENDER_BLEND_FACTOR_ZERO,
            .destination_alpha_blend_factor = KAN_RENDER_BLEND_FACTOR_ZERO,
            .alpha_blend_operation = KAN_RENDER_BLEND_OPERATION_MAX,
        },
    };

    struct kan_render_pipeline_code_entry_point_t code_entry_points[2u] = {
        {
            .stage = KAN_RENDER_STAGE_GRAPHICS_VERTEX,
            .function_name = kan_string_intern ("vertex_main"),
        },
        {
            .stage = KAN_RENDER_STAGE_GRAPHICS_FRAGMENT,
            .function_name = kan_string_intern ("fragment_main"),
        },
    };

    struct kan_render_pipeline_code_module_usage_t code_modules[1u] = {
        {
            .code_module = code_module,
            .entry_points_count = sizeof (code_entry_points) / sizeof (code_entry_points[0u]),
            .entry_points = code_entry_points,
        },
    };

    struct kan_render_graphics_pipeline_description_t pipeline_description = {
        .pass = render_image_pass,
        .family = family,
        .polygon_mode = KAN_RENDER_POLYGON_MODE_FILL,
        .cull_mode = KAN_RENDER_CULL_MODE_BACK,
        .use_depth_clamp = KAN_FALSE,
        .output_setups_count = sizeof (output_setups) / sizeof (output_setups[0u]),
        .output_setups = output_setups,
        .blend_constant_r = 0.0f,
        .blend_constant_g = 0.0f,
        .blend_constant_b = 0.0f,
        .blend_constant_a = 0.0f,
        .depth_test_enabled = KAN_FALSE,
        .depth_write_enabled = KAN_FALSE,
        .depth_bounds_test_enabled = KAN_FALSE,
        .depth_compare_operation = KAN_RENDER_COMPARE_OPERATION_LESS,
        .min_depth = 0.0f,
        .max_depth = 1.0f,
        .stencil_test_enabled = KAN_FALSE,
        .stencil_front =
            {
                .on_fail = KAN_RENDER_STENCIL_OPERATION_KEEP,
                .on_depth_fail = KAN_RENDER_STENCIL_OPERATION_KEEP,
                .on_pass = KAN_RENDER_STENCIL_OPERATION_KEEP,
                .compare = KAN_RENDER_COMPARE_OPERATION_LESS,
                .compare_mask = 255u,
                .write_mask = 255u,
                .reference = 255u,
            },
        .stencil_back =
            {
                .on_fail = KAN_RENDER_STENCIL_OPERATION_KEEP,
                .on_depth_fail = KAN_RENDER_STENCIL_OPERATION_KEEP,
                .on_pass = KAN_RENDER_STENCIL_OPERATION_KEEP,
                .compare = KAN_RENDER_COMPARE_OPERATION_LESS,
                .compare_mask = 255u,
                .write_mask = 255u,
                .reference = 255u,
            },
        .code_modules_count = sizeof (code_modules) / sizeof (code_modules[0u]),
        .code_modules = code_modules,
        .samplers_count = 0u,
        .samplers = NULL,
        .tracking_name = kan_string_intern ("render_image_pipeline"),
    };

    kan_render_graphics_pipeline_t pipeline = kan_render_graphics_pipeline_create (
        render_context, &pipeline_description, KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_ACTIVE);

    kan_rpl_meta_shutdown (&meta);
    kan_dynamic_array_shutdown (&code);

    return pipeline;
}

struct cube_vertex_t
{
    struct kan_float_vector_3_t position;
    struct kan_float_vector_2_t uv;
};

struct cube_instanced_t
{
    struct kan_float_matrix_4x4_t model;
};

struct pass_t
{
    struct kan_float_matrix_4x4_t projection_view;
};

static const char *cube_shader =
    "vertex_attribute_buffer vertex\n"
    "{\n"
    "    f3 position;\n"
    "    f2 uv;\n"
    "};\n"
    "\n"
    "instanced_attribute_buffer instanced\n"
    "{\n"
    "    f4x4 model;\n"
    "};\n"
    "\n"
    "uniform_buffer pass\n"
    "{\n"
    "    f4x4 projection_view;\n"
    "};\n"
    "\n"
    "vertex_stage_output vertex_output\n"
    "{\n"
    "    f2 uv;\n"
    "};\n"
    "\n"
    "void vertex_main (void)\n"
    "{\n"
    "    vertex_output.uv = vertex.uv;"
    "    vertex_stage_output_position (\n"
    "        pass.projection_view * instanced.model * expand_f3_to_f4 (vertex.position, 1.0));\n"
    "}\n"
    "\n"
    "sampler_2d diffuse_color\n"
    "{\n"
    "    setting mag_filter \"nearest\";\n"
    "    setting min_filter \"nearest\";\n"
    "    setting mip_map_mode \"nearest\";\n"
    "    setting address_mode_u \"repeat\";\n"
    "    setting address_mode_v \"repeat\";\n"
    "    setting address_mode_w \"repeat\";\n"
    "};\n"
    "\n"
    "fragment_stage_output fragment_output\n"
    "{\n"
    "    f4 color;\n"
    "};\n"
    "\n"
    "void fragment_main (void)\n"
    "{\n"
    "    fragment_output.color = diffuse_color (vertex_output.uv);\n"
    "}\n";

static kan_render_pass_t create_cube_pass (kan_render_context_t render_context)
{
    struct kan_render_pass_attachment_t attachments[] = {
        {
            .type = KAN_RENDER_PASS_ATTACHMENT_COLOR,
            .color_format = KAN_RENDER_COLOR_FORMAT_SURFACE,
            .samples = 1u,
            .load_operation = KAN_RENDER_LOAD_OPERATION_CLEAR,
            .store_operation = KAN_RENDER_STORE_OPERATION_STORE,
        },
        {
            .type = KAN_RENDER_PASS_ATTACHMENT_DEPTH,
            .samples = 1u,
            .load_operation = KAN_RENDER_LOAD_OPERATION_CLEAR,
            .store_operation = KAN_RENDER_STORE_OPERATION_ANY,
        },
    };

    struct kan_render_pass_description_t description = {
        .type = KAN_RENDER_PASS_GRAPHICS,
        .attachments_count = sizeof (attachments) / sizeof (attachments[0u]),
        .attachments = attachments,
        .tracking_name = kan_string_intern ("cube_pass"),
    };

    return kan_render_pass_create (render_context, &description);
}

static kan_render_graphics_pipeline_t create_cube_pipeline (kan_render_context_t render_context,
                                                            kan_render_pass_t cube_pass,
                                                            uint64_t *output_attribute_vertex_binding,
                                                            uint64_t *output_attribute_instanced_binding,
                                                            uint64_t *output_parameter_set,
                                                            uint64_t *output_pass_binding,
                                                            uint64_t *output_diffuse_color_binding)
{
    kan_rpl_parser_t parser = kan_rpl_parser_create (kan_string_intern ("cube"));
    KAN_TEST_ASSERT (kan_rpl_parser_add_source (parser, cube_shader, kan_string_intern ("cube")))
    struct kan_rpl_intermediate_t intermediate;
    kan_rpl_intermediate_init (&intermediate);
    KAN_TEST_ASSERT (kan_rpl_parser_build_intermediate (parser, &intermediate))
    kan_rpl_parser_destroy (parser);

    kan_rpl_compiler_context_t compiler_context =
        kan_rpl_compiler_context_create (KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC, kan_string_intern ("cube"));
    kan_rpl_compiler_context_use_module (compiler_context, &intermediate);

    struct kan_dynamic_array_t code;
    struct kan_rpl_meta_t meta;
    kan_rpl_meta_init (&meta);

    struct kan_rpl_entry_point_t entry_points[] = {
        {
            .stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX,
            .function_name = kan_string_intern ("vertex_main"),
        },
        {
            .stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT,
            .function_name = kan_string_intern ("fragment_main"),
        },
    };

    kan_rpl_compiler_instance_t compiler_instance = kan_rpl_compiler_context_resolve (
        compiler_context, sizeof (entry_points) / sizeof (entry_points[0u]), entry_points);
    KAN_TEST_ASSERT (compiler_instance != KAN_INVALID_RPL_COMPILER_INSTANCE)

    KAN_TEST_ASSERT (
        kan_render_backend_tools_emit_platform_code (compiler_instance, &code, KAN_ALLOCATION_GROUP_IGNORE))
    KAN_TEST_ASSERT (kan_rpl_compiler_instance_emit_meta (compiler_instance, &meta))
    kan_rpl_compiler_instance_destroy (compiler_instance);

    kan_rpl_compiler_context_destroy (compiler_context);
    kan_rpl_intermediate_shutdown (&intermediate);

    struct kan_render_attribute_source_description_t attribute_sources[2u];
    uint64_t attribute_source_index = 0u;

    struct kan_render_attribute_description_t attributes[3u];
    uint64_t attribute_index = 0u;

    struct kan_render_parameter_binding_description_t parameter_set_bindings[2u];
    struct kan_render_parameter_set_description_t parameter_sets[1u];

    for (uint64_t buffer_index = 0u; buffer_index < meta.buffers.size; ++buffer_index)
    {
        struct kan_rpl_meta_buffer_t *buffer = &((struct kan_rpl_meta_buffer_t *) meta.buffers.data)[buffer_index];
        if (buffer->name == kan_string_intern ("vertex"))
        {
            attribute_sources[attribute_source_index].binding = buffer->binding;
            *output_attribute_vertex_binding = buffer->binding;
            attribute_sources[attribute_source_index].stride = buffer->size;
            attribute_sources[attribute_source_index].rate = KAN_RENDER_ATTRIBUTE_RATE_PER_VERTEX;
            ++attribute_source_index;

            KAN_TEST_ASSERT (buffer->attributes.size == 2u)
            struct kan_rpl_meta_attribute_t *attribute =
                &((struct kan_rpl_meta_attribute_t *) buffer->attributes.data)[0u];

            attributes[attribute_index].binding = buffer->binding;
            attributes[attribute_index].location = attribute->location;
            attributes[attribute_index].offset = attribute->offset;
            KAN_TEST_CHECK (attribute->type == KAN_RPL_META_VARIABLE_TYPE_F3)
            attributes[attribute_index].format = KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_3;
            ++attribute_index;

            attribute = &((struct kan_rpl_meta_attribute_t *) buffer->attributes.data)[1u];
            attributes[attribute_index].binding = buffer->binding;
            attributes[attribute_index].location = attribute->location;
            attributes[attribute_index].offset = attribute->offset;
            KAN_TEST_CHECK (attribute->type == KAN_RPL_META_VARIABLE_TYPE_F2)
            attributes[attribute_index].format = KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_2;
            ++attribute_index;
        }
        else if (buffer->name == kan_string_intern ("instanced"))
        {
            attribute_sources[attribute_source_index].binding = buffer->binding;
            *output_attribute_instanced_binding = buffer->binding;
            attribute_sources[attribute_source_index].stride = buffer->size;
            attribute_sources[attribute_source_index].rate = KAN_RENDER_ATTRIBUTE_RATE_PER_INSTANCE;
            ++attribute_source_index;

            KAN_TEST_ASSERT (buffer->attributes.size == 1u)
            struct kan_rpl_meta_attribute_t *attribute =
                &((struct kan_rpl_meta_attribute_t *) buffer->attributes.data)[0u];

            attributes[attribute_index].binding = buffer->binding;
            attributes[attribute_index].location = attribute->location;
            attributes[attribute_index].offset = attribute->offset;
            KAN_TEST_CHECK (attribute->type == KAN_RPL_META_VARIABLE_TYPE_F4X4)
            attributes[attribute_index].format = KAN_RENDER_ATTRIBUTE_FORMAT_MATRIX_FLOAT_4_4;
            ++attribute_index;
        }
        else if (buffer->name == kan_string_intern ("pass"))
        {
            parameter_sets[0u].set = buffer->set;
            *output_parameter_set = buffer->set;
            parameter_sets[0u].bindings_count = sizeof (parameter_set_bindings) / sizeof (parameter_set_bindings[0u]);
            parameter_sets[0u].bindings = parameter_set_bindings;
            parameter_sets[0u].stable_binding = KAN_TRUE;

            KAN_TEST_CHECK (buffer->type == KAN_RPL_BUFFER_TYPE_UNIFORM)
            parameter_set_bindings[0u].binding = buffer->binding;
            *output_pass_binding = buffer->binding;
            parameter_set_bindings[0u].type = KAN_RENDER_PARAMETER_BINDING_TYPE_UNIFORM_BUFFER;
            parameter_set_bindings[0u].used_stage_mask =
                (1u << KAN_RENDER_STAGE_GRAPHICS_VERTEX) | (1u << KAN_RENDER_STAGE_GRAPHICS_FRAGMENT);
        }
        else
        {
            KAN_TEST_CHECK (KAN_FALSE)
        }
    }

    for (uint64_t sampler_index = 0u; sampler_index < meta.samplers.size; ++sampler_index)
    {
        struct kan_rpl_meta_sampler_t *sampler = &((struct kan_rpl_meta_sampler_t *) meta.samplers.data)[sampler_index];
        if (sampler->name == kan_string_intern ("diffuse_color"))
        {
            KAN_TEST_ASSERT (*output_parameter_set == sampler->set)
            parameter_set_bindings[1u].binding = sampler->binding;
            *output_diffuse_color_binding = sampler->binding;
            parameter_set_bindings[1u].type = KAN_RENDER_PARAMETER_BINDING_TYPE_COMBINED_IMAGE_SAMPLER;
            parameter_set_bindings[1u].used_stage_mask =
                (1u << KAN_RENDER_STAGE_GRAPHICS_VERTEX) | (1u << KAN_RENDER_STAGE_GRAPHICS_FRAGMENT);
        }
        else
        {
            KAN_TEST_CHECK (KAN_FALSE)
        }
    }

    struct kan_render_graphics_pipeline_family_description_t pipeline_family_description = {
        .topology = KAN_RENDER_GRAPHICS_TOPOLOGY_TRIANGLE_LIST,
        .attribute_sources_count = sizeof (attribute_sources) / (sizeof (attribute_sources[0u])),
        .attribute_sources = attribute_sources,
        .attributes_count = sizeof (attributes) / (sizeof (attributes[0u])),
        .attributes = attributes,
        .parameter_sets_count = sizeof (parameter_sets) / sizeof (parameter_sets[0u]),
        .parameter_sets = parameter_sets,
        .tracking_name = "cube_pipeline_family",
    };

    kan_render_graphics_pipeline_family_t family =
        kan_render_graphics_pipeline_family_create (render_context, &pipeline_family_description);
    KAN_TEST_ASSERT (family != KAN_INVALID_RENDER_GRAPHICS_PIPELINE_FAMILY)

    kan_render_code_module_t code_module = kan_render_code_module_create (render_context, code.size * code.item_size,
                                                                          code.data, kan_string_intern ("cube_code"));

    struct kan_render_color_output_setup_description_t output_setups[1u] = {
        {
            .use_blend = KAN_FALSE,
            .write_r = KAN_TRUE,
            .write_g = KAN_TRUE,
            .write_b = KAN_TRUE,
            .write_a = KAN_TRUE,
            .source_color_blend_factor = KAN_RENDER_BLEND_FACTOR_ZERO,
            .destination_color_blend_factor = KAN_RENDER_BLEND_FACTOR_ZERO,
            .color_blend_operation = KAN_RENDER_BLEND_OPERATION_MAX,
            .source_alpha_blend_factor = KAN_RENDER_BLEND_FACTOR_ZERO,
            .destination_alpha_blend_factor = KAN_RENDER_BLEND_FACTOR_ZERO,
            .alpha_blend_operation = KAN_RENDER_BLEND_OPERATION_MAX,
        },
    };

    struct kan_render_pipeline_code_entry_point_t code_entry_points[2u] = {
        {
            .stage = KAN_RENDER_STAGE_GRAPHICS_VERTEX,
            .function_name = kan_string_intern ("vertex_main"),
        },
        {
            .stage = KAN_RENDER_STAGE_GRAPHICS_FRAGMENT,
            .function_name = kan_string_intern ("fragment_main"),
        },
    };

    struct kan_render_pipeline_code_module_usage_t code_modules[1u] = {
        {
            .code_module = code_module,
            .entry_points_count = sizeof (code_entry_points) / sizeof (code_entry_points[0u]),
            .entry_points = code_entry_points,
        },
    };

    struct kan_render_sampler_description_t samplers[] = {
        {
            .parameter_set = *output_parameter_set,
            .parameter_binding = *output_diffuse_color_binding,
            .mag_filter = KAN_RENDER_FILTER_MODE_NEAREST,
            .min_filter = KAN_RENDER_FILTER_MODE_NEAREST,
            .mip_map_mode = KAN_RENDER_MIP_MAP_MODE_NEAREST,
            .address_mode_u = KAN_RENDER_ADDRESS_MODE_REPEAT,
            .address_mode_v = KAN_RENDER_ADDRESS_MODE_REPEAT,
            .address_mode_w = KAN_RENDER_ADDRESS_MODE_REPEAT,
        },
    };

    struct kan_render_graphics_pipeline_description_t pipeline_description = {
        .pass = cube_pass,
        .family = family,
        .polygon_mode = KAN_RENDER_POLYGON_MODE_FILL,
        .cull_mode = KAN_RENDER_CULL_MODE_BACK,
        .use_depth_clamp = KAN_FALSE,
        .output_setups_count = sizeof (output_setups) / sizeof (output_setups[0u]),
        .output_setups = output_setups,
        .blend_constant_r = 0.0f,
        .blend_constant_g = 0.0f,
        .blend_constant_b = 0.0f,
        .blend_constant_a = 0.0f,
        .depth_test_enabled = KAN_TRUE,
        .depth_write_enabled = KAN_TRUE,
        .depth_bounds_test_enabled = KAN_FALSE,
        .depth_compare_operation = KAN_RENDER_COMPARE_OPERATION_LESS,
        .min_depth = 0.0f,
        .max_depth = 1.0f,
        .stencil_test_enabled = KAN_FALSE,
        .stencil_front =
            {
                .on_fail = KAN_RENDER_STENCIL_OPERATION_KEEP,
                .on_depth_fail = KAN_RENDER_STENCIL_OPERATION_KEEP,
                .on_pass = KAN_RENDER_STENCIL_OPERATION_KEEP,
                .compare = KAN_RENDER_COMPARE_OPERATION_LESS,
                .compare_mask = 255u,
                .write_mask = 255u,
                .reference = 255u,
            },
        .stencil_back =
            {
                .on_fail = KAN_RENDER_STENCIL_OPERATION_KEEP,
                .on_depth_fail = KAN_RENDER_STENCIL_OPERATION_KEEP,
                .on_pass = KAN_RENDER_STENCIL_OPERATION_KEEP,
                .compare = KAN_RENDER_COMPARE_OPERATION_LESS,
                .compare_mask = 255u,
                .write_mask = 255u,
                .reference = 255u,
            },
        .code_modules_count = sizeof (code_modules) / sizeof (code_modules[0u]),
        .code_modules = code_modules,
        .samplers_count = sizeof (samplers) / sizeof (samplers[0u]),
        .samplers = samplers,
        .tracking_name = kan_string_intern ("cube_pipeline"),
    };

    kan_render_graphics_pipeline_t pipeline = kan_render_graphics_pipeline_create (
        render_context, &pipeline_description, KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_ACTIVE);

    kan_rpl_meta_shutdown (&meta);
    kan_dynamic_array_shutdown (&code);

    return pipeline;
}

KAN_TEST_CASE (temp)
{
    kan_platform_application_init ();
    kan_context_handle_t context =
        kan_context_create (kan_allocation_group_get_child (kan_allocation_group_root (), "context"));

    struct kan_render_backend_system_config_t render_backend_config = {
        .disable_render = KAN_FALSE,
        .prefer_vsync = KAN_FALSE,
        .application_info_name = kan_string_intern ("Kan autotest"),
        .version_major = 1u,
        .version_minor = 0u,
        .version_patch = 0u,
    };

    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (
        kan_context_request_system (context, KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME, &render_backend_config))

    kan_context_assembly (context);
    kan_context_system_handle_t application_system = kan_context_query (context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);
    kan_context_system_handle_t render_backend_system =
        kan_context_query (context, KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME);
    kan_render_context_t render_context = kan_render_backend_system_get_render_context (render_backend_system);

    struct kan_render_supported_devices_t *devices = kan_render_backend_system_get_devices (render_backend_system);
    printf ("Devices (%lu):\n", (unsigned long) devices->supported_device_count);
    kan_render_device_id_t picked_device;
    uint64_t picked_device_index = UINT64_MAX;

    for (uint64_t index = 0u; index < devices->supported_device_count; ++index)
    {
        printf ("  - name: %s\n    device_type: %lu\n    memory_type: %lu\n", devices->devices[index].name,
                (unsigned long) devices->devices[index].device_type,
                (unsigned long) devices->devices[index].memory_type);

        if (picked_device_index == UINT64_MAX ||
            devices->devices[picked_device_index].device_type != KAN_RENDER_DEVICE_TYPE_DISCRETE_GPU)
        {
            picked_device = devices->devices[index].id;
            picked_device_index = index;
        }
    }

    kan_render_backend_system_select_device (render_backend_system, picked_device);
    const uint64_t base_window_size = 512u;
    kan_application_system_window_handle_t window_handle = kan_application_system_window_create (
        application_system, "Kan context_render_backend test window", base_window_size, base_window_size,
        kan_render_get_required_window_flags () | KAN_PLATFORM_WINDOW_FLAG_RESIZABLE);

    const struct kan_application_system_window_info_t *window_info =
        kan_application_system_get_window_info_from_handle (application_system, window_handle);

    kan_application_system_event_iterator_t event_iterator =
        kan_application_system_event_iterator_create (application_system);

    kan_render_surface_t test_surface = kan_render_backend_system_create_surface (render_backend_system, window_handle,
                                                                                  kan_string_intern ("test_surface"));

    kan_render_pass_t render_image_pass = create_render_image_pass (render_context);
    KAN_TEST_ASSERT (render_image_pass != KAN_INVALID_RENDER_PASS)

    kan_render_pass_t cube_pass = create_cube_pass (render_context);
    KAN_TEST_ASSERT (cube_pass != KAN_INVALID_RENDER_PASS)

    uint64_t render_image_attribute_binding;
    uint64_t render_image_config_set;
    uint64_t render_image_config_binding;

    kan_render_graphics_pipeline_t render_image_pipeline =
        create_render_image_pipeline (render_context, render_image_pass, &render_image_attribute_binding,
                                      &render_image_config_set, &render_image_config_binding);
    KAN_TEST_ASSERT (render_image_pipeline != KAN_INVALID_RENDER_GRAPHICS_PIPELINE)

    uint64_t cube_attribute_vertex_binding;
    uint64_t cube_attribute_instanced_binding;
    uint64_t cube_parameter_set;
    uint64_t cube_pass_binding;
    uint64_t cube_diffuse_color_binding;

    kan_render_graphics_pipeline_t cube_pipeline = create_cube_pipeline (
        render_context, cube_pass, &cube_attribute_vertex_binding, &cube_attribute_instanced_binding,
        &cube_parameter_set, &cube_pass_binding, &cube_diffuse_color_binding);

    const uint64_t render_target_image_size = 256u;
    struct kan_render_image_description_t render_target_image_description = {
        .type = KAN_RENDER_IMAGE_TYPE_COLOR_2D,
        .color_format = KAN_RENDER_COLOR_FORMAT_RGBA32_SRGB,
        .width = render_target_image_size,
        .height = render_target_image_size,
        .depth = 1u,
        .mips = 1u,
        .render_target = KAN_TRUE,
        .supports_sampling = KAN_TRUE,
        .tracking_name = kan_string_intern ("render_target_image"),
    };

    kan_render_image_t render_target_image = kan_render_image_create (render_context, &render_target_image_description);

    struct kan_render_image_description_t depth_image_description = {
        .type = KAN_RENDER_IMAGE_TYPE_DEPTH,
        .width = base_window_size,
        .height = base_window_size,
        .depth = 1u,
        .mips = 1u,
        .render_target = KAN_TRUE,
        .supports_sampling = KAN_FALSE,
        .tracking_name = kan_string_intern ("depth_image"),
    };

    kan_render_image_t depth_image = kan_render_image_create (render_context, &depth_image_description);

    struct kan_render_frame_buffer_attachment_description_t render_image_frame_buffer_attachments[] = {
        {
            .type = KAN_FRAME_BUFFER_ATTACHMENT_IMAGE,
            .image = render_target_image,
        },
    };

    struct kan_render_frame_buffer_description_t render_image_frame_buffer_description = {
        .associated_pass = render_image_pass,
        .attachment_count =
            sizeof (render_image_frame_buffer_attachments) / sizeof (render_image_frame_buffer_attachments[0u]),
        .attachments = render_image_frame_buffer_attachments,
        .tracking_name = kan_string_intern ("render_image_frame_buffer"),
    };

    kan_render_frame_buffer_t render_image_frame_buffer =
        kan_render_frame_buffer_create (render_context, &render_image_frame_buffer_description);

    struct kan_render_frame_buffer_attachment_description_t surface_frame_buffer_attachments[] = {
        {
            .type = KAN_FRAME_BUFFER_ATTACHMENT_SURFACE,
            .surface = test_surface,
        },
        {
            .type = KAN_FRAME_BUFFER_ATTACHMENT_IMAGE,
            .image = depth_image,
        },
    };

    struct kan_render_frame_buffer_description_t surface_frame_buffer_description = {
        .associated_pass = cube_pass,
        .attachment_count = sizeof (surface_frame_buffer_attachments) / sizeof (surface_frame_buffer_attachments[0u]),
        .attachments = surface_frame_buffer_attachments,
        .tracking_name = kan_string_intern ("surface_frame_buffer"),
    };

    kan_render_frame_buffer_t surface_frame_buffer =
        kan_render_frame_buffer_create (render_context, &surface_frame_buffer_description);

    float render_image_quad_vertices[] = {
        -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f,
    };

    kan_render_buffer_t render_image_quad_vertex_buffer =
        kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_ATTRIBUTE, sizeof (render_image_quad_vertices),
                                  render_image_quad_vertices, kan_string_intern ("render_image_quad_vertices"));

    uint16_t render_image_quad_indices[] = {
        0u, 1u, 2u, 2u, 3u, 0u,
    };

    kan_render_buffer_t render_image_quad_index_buffer =
        kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_INDEX_16, sizeof (render_image_quad_indices),
                                  render_image_quad_indices, kan_string_intern ("render_image_quad_indices"));

    struct render_image_config_t render_image_config = {
        .bricks_count_horizontal = 3.0f,
        .bricks_count_vertical = 7.0f,
        .border_size_horizontal = 0.02f,
        .border_size_vertical = 0.05f,
        .brick_color_r = 0.88f,
        .brick_color_g = 0.33f,
        .brick_color_b = 0.04f,
        .brick_color_a = 1.0f,
        .border_color_r = 0.59f,
        .border_color_g = 0.59f,
        .border_color_b = 0.59f,
        .border_color_a = 1.0f,
        .image_border_r = 0.05f,
        .image_border_g = 0.05f,
        .image_border_b = 0.05f,
        .image_border_a = 1.0f,
        .image_border_size = 0.03f,
    };

    kan_render_buffer_t render_image_config_buffer =
        kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_STORAGE, sizeof (render_image_config),
                                  &render_image_config, kan_string_intern ("render_image_config"));

    struct cube_vertex_t cube_vertices[] = {
        {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f}},   {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f}},
        {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}},     {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f}}, {{-0.5f, 0.5f, -0.5f}, {-1.0f, 1.0f}},
        {{0.5f, -0.5f, -0.5f}, {2.0f, 0.0f}},   {{0.5f, 0.5f, -0.5f}, {2.0f, 1.0f}},
        {{0.5f, 0.5f, -0.5f}, {1.0f, 2.0f}},    {{-0.5f, 0.5f, -0.5f}, {0.0f, 2.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f}}, {{0.5f, -0.5f, -0.5f}, {1.0f, -1.0f}},
        {{-0.5f, 0.5f, -0.5f}, {0.0f, -2.0f}},  {{0.5f, 0.5f, -0.5f}, {1.0f, -2.0f}},
    };

    kan_render_buffer_t cube_vertex_buffer =
        kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_ATTRIBUTE, sizeof (cube_vertices),
                                  cube_vertices, kan_string_intern ("cube_vertices"));

    uint16_t cube_indices[] = {0u, 1u, 2u, 2u, 3u, 0u, 0u,  3u,  5u, 5u, 4u, 0u,  1u,  6u,  7u,  7u,  2u,  1u,
                               2u, 8u, 9u, 9u, 3u, 2u, 10u, 11u, 1u, 1u, 0u, 10u, 12u, 13u, 11u, 11u, 10u, 12u};

    kan_render_buffer_t cube_index_buffer =
        kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_INDEX_16, sizeof (cube_indices), cube_indices,
                                  kan_string_intern ("cube_indices"));

#define INSTANCED_CUBES_X 8u
#define INSTANCED_CUBES_Y 8u
#define INSTANCED_CUBES_Z 8u
#define MAX_INSTANCED_CUBES (INSTANCED_CUBES_X * INSTANCED_CUBES_Y * INSTANCED_CUBES_Z)

    struct cube_instanced_t cube_instanced_data[MAX_INSTANCED_CUBES];
    kan_render_buffer_t cube_instanced_buffer =
        kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_ATTRIBUTE, sizeof (cube_instanced_data), NULL,
                                  kan_string_intern ("cube_indices"));

    struct pass_t pass_data;
    kan_render_buffer_t pass_buffer = kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_UNIFORM,
                                                                sizeof (pass_data), NULL, kan_string_intern ("pass"));

    struct kan_render_parameter_update_description_t render_image_parameters[] = {{
        .binding = render_image_config_binding,
        .buffer_binding =
            {
                .buffer = render_image_config_buffer,
                .offset = 0u,
                .range = sizeof (render_image_config),
            },
    }};

    struct kan_render_pipeline_parameter_set_description_t render_image_set_description = {
        .pipeline_type = KAN_RENDER_PIPELINE_TYPE_GRAPHICS,
        .graphics_pipeline = render_image_pipeline,
        .set = render_image_config_set,
        .tracking_name = kan_string_intern ("render_image_parameter_set"),
        .initial_bindings_count = sizeof (render_image_parameters) / sizeof (render_image_parameters[0u]),
        .initial_bindings = render_image_parameters,
    };

    kan_render_pipeline_parameter_set_t render_image_set =
        kan_render_pipeline_parameter_set_create (render_context, &render_image_set_description);

    struct kan_render_parameter_update_description_t cube_parameters[] = {
        {
            .binding = cube_pass_binding,
            .buffer_binding =
                {
                    .buffer = pass_buffer,
                    .offset = 0u,
                    .range = sizeof (pass_data),
                },
        },
        {
            .binding = cube_diffuse_color_binding,
            .image_binding =
                {
                    .image = render_target_image,
                },
        },
    };

    struct kan_render_pipeline_parameter_set_description_t cube_set_description = {
        .pipeline_type = KAN_RENDER_PIPELINE_TYPE_GRAPHICS,
        .graphics_pipeline = cube_pipeline,
        .set = cube_parameter_set,
        .tracking_name = kan_string_intern ("cube_parameter_set"),
        .initial_bindings_count = sizeof (cube_parameters) / sizeof (cube_parameters[0u]),
        .initial_bindings = cube_parameters,
    };

    kan_render_pipeline_parameter_set_t cube_set =
        kan_render_pipeline_parameter_set_create (render_context, &cube_set_description);

    uint64_t width = base_window_size;
    uint64_t height = base_window_size;
    uint64_t last_render_image_frame = UINT64_MAX;
    kan_bool_t exit_requested = KAN_FALSE;

    // #define TEST_FRAMES 600u
    for (uint64_t frame = 0u; !exit_requested /*frame < TEST_FRAMES*/; ++frame)
    {
        const uint64_t start_time = kan_platform_get_elapsed_nanoseconds ();
        kan_application_system_sync_in_main_thread (application_system);

        const struct kan_platform_application_event_t *event;
        while ((event = kan_application_system_event_iterator_get (application_system, event_iterator)))
        {
            if (event->type == KAN_PLATFORM_APPLICATION_EVENT_TYPE_QUIT)
            {
                exit_requested = KAN_TRUE;
            }

            event_iterator = kan_application_system_event_iterator_advance (event_iterator);
        }

        if (width != window_info->width_for_render || height != window_info->height_for_render)
        {
            width = window_info->width_for_render;
            height = window_info->height_for_render;
            kan_render_image_resize_render_target (depth_image, width, height, 1u);
        }

        if (kan_render_backend_system_next_frame (render_backend_system))
        {
            struct kan_render_viewport_bounds_t cube_viewport_bounds = {
                .x = 0.0f,
                .y = 0.0f,
                .width = (float) window_info->width_for_render,
                .height = (float) window_info->height_for_render,
                .depth_min = 0.0f,
                .depth_max = 1.0f,
            };

            struct kan_render_integer_region_t cube_scissor = {
                .x = 0,
                .y = 0,
                .width = window_info->width_for_render,
                .height = window_info->height_for_render,
            };

            struct kan_render_clear_value_t cube_attachment_clear_values[] = {
                {
                    .color = {0.0f, 0.0f, 0.0f, 1.0f},
                },
                {
                    .depth_stencil = {1.0f, 0u},
                },
            };

            kan_render_pass_instance_t cube_instance = kan_render_pass_instantiate (
                cube_pass, surface_frame_buffer, &cube_viewport_bounds, &cube_scissor, cube_attachment_clear_values);

            if (cube_instance != KAN_INVALID_RENDER_PASS_INSTANCE)
            {
                struct kan_float_matrix_4x4_t projection;
                kan_perspective_projection (&projection, KAN_PI_2, ((float) width) / ((float) height), 0.01f, 5000.0f);

                struct kan_transform_3_t camera_transform = kan_transform_3_get_identity ();
                camera_transform.location.y = 10.0f;
                camera_transform.location.z = -15.0f;
                camera_transform.rotation = kan_make_quaternion_from_euler (KAN_PI / 6.0f, 0.0f, 0.0f);

                struct kan_float_matrix_4x4_t camera_transform_matrix;
                kan_transform_3_to_float_matrix_4x4 (&camera_transform, &camera_transform_matrix);

                struct kan_float_matrix_4x4_t view;
                kan_float_matrix_4x4_inverse (&camera_transform_matrix, &view);

                kan_float_matrix_4x4_multiply (&projection, &view, &pass_data.projection_view);
                void *pass_memory = kan_render_buffer_patch (pass_buffer, 0u, sizeof (pass_data));
                KAN_TEST_ASSERT (pass_memory)
                memcpy (pass_memory, &pass_data, sizeof (pass_data));

                for (uint32_t x = 0u; x < INSTANCED_CUBES_X; ++x)
                {
                    for (uint32_t y = 0u; y < INSTANCED_CUBES_Y; ++y)
                    {
                        for (uint32_t z = 0u; z < INSTANCED_CUBES_Z; ++z)
                        {
                            const uint32_t index =
                                x * INSTANCED_CUBES_Y * INSTANCED_CUBES_Z + y * INSTANCED_CUBES_Z + z;
                            struct kan_transform_3_t transform = kan_transform_3_get_identity ();
                            transform.location.x = ((float) x) * 2.0f - 7.0f;
                            transform.location.y = ((float) y) * 2.0f - 7.0f;
                            transform.location.z = ((float) z) * 2.0f - 7.0f;
                            transform.rotation = kan_make_quaternion_from_euler (
                                0.0f, (float) ((index + frame) % 120u) * KAN_PI / 120.0f, 0.0f);
                            kan_transform_3_to_float_matrix_4x4 (&transform, &cube_instanced_data[index].model);
                        }
                    }
                }

                void *cube_instanced_memory =
                    kan_render_buffer_patch (cube_instanced_buffer, 0u, sizeof (cube_instanced_data));
                KAN_TEST_ASSERT (cube_instanced_memory)
                memcpy (cube_instanced_memory, &cube_instanced_data, sizeof (cube_instanced_data));

                kan_render_pass_instance_graphics_pipeline (cube_instance, cube_pipeline);
                kan_render_pass_instance_pipeline_parameter_sets (cube_instance, 1u, &cube_set);
                kan_render_pass_instance_attributes (cube_instance, (uint32_t) cube_attribute_vertex_binding, 1u,
                                                     &cube_vertex_buffer);
                kan_render_pass_instance_attributes (cube_instance, (uint32_t) cube_attribute_instanced_binding, 1u,
                                                     &cube_instanced_buffer);
                kan_render_pass_instance_indices (cube_instance, cube_index_buffer);
                kan_render_pass_instance_instanced_draw (cube_instance, 0u,
                                                         (uint32_t) (sizeof (cube_indices) / sizeof (cube_indices[0u])),
                                                         0u, 0u, MAX_INSTANCED_CUBES);
            }

            if (last_render_image_frame == UINT64_MAX || frame - last_render_image_frame >= 5u)
            {
                struct kan_render_viewport_bounds_t render_image_viewport_bounds = {
                    .x = 0.0f,
                    .y = 0.0f,
                    .width = (float) render_target_image_size,
                    .height = (float) render_target_image_size,
                    .depth_min = 0.0f,
                    .depth_max = 1.0f,
                };

                struct kan_render_integer_region_t render_image_scissor = {
                    .x = 0,
                    .y = 0,
                    .width = render_target_image_size,
                    .height = render_target_image_size,
                };

                struct kan_render_clear_value_t render_image_attachment_clear_values[] = {
                    {
                        .color = {0.0f, 0.0f, 0.0f, 1.0f},
                    },
                };

                kan_render_pass_instance_t render_image_instance = kan_render_pass_instantiate (
                    render_image_pass, render_image_frame_buffer, &render_image_viewport_bounds, &render_image_scissor,
                    render_image_attachment_clear_values);

                if (render_image_instance != KAN_INVALID_RENDER_PASS_INSTANCE)
                {
                    kan_render_pass_instance_graphics_pipeline (render_image_instance, render_image_pipeline);
                    kan_render_pass_instance_pipeline_parameter_sets (render_image_instance, 1u, &render_image_set);
                    kan_render_pass_instance_attributes (render_image_instance,
                                                         (uint32_t) render_image_attribute_binding, 1u,
                                                         &render_image_quad_vertex_buffer);
                    kan_render_pass_instance_indices (render_image_instance, render_image_quad_index_buffer);
                    kan_render_pass_instance_draw (
                        render_image_instance, 0u,
                        (uint32_t) (sizeof (render_image_quad_indices) / sizeof (render_image_quad_indices[0u])), 0u);

                    if (cube_instance != KAN_INVALID_RENDER_PASS_INSTANCE)
                    {
                        kan_render_pass_instance_add_dynamic_dependency (cube_instance, render_image_instance);
                    }

                    last_render_image_frame = frame;
                }
            }
        }

        const uint64_t end_time = kan_platform_get_elapsed_nanoseconds ();
        const uint64_t duration = end_time - start_time;

        if (duration < 8000000u)
        {
            // Sleep to avoid exiting too fast.
            kan_platform_sleep (8000000u - duration);
        }
    }

    kan_application_system_event_iterator_destroy (application_system, event_iterator);
    kan_application_system_prepare_for_destroy_in_main_thread (application_system);

#undef TEST_FRAMES
    kan_context_destroy (context);
    kan_platform_application_shutdown ();
}
