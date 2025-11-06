#include <string.h>

#include <kan/context/all_system_names.h>
#include <kan/file_system/stream.h>
#include <kan/image/image.h>
#include <kan/memory/allocation.h>
#include <kan/precise_time/precise_time.h>
#include <kan/render_pipeline_language/compiler.h>
#include <kan/render_pipeline_language/parser.h>
#include <kan/testing/testing.h>
#include <kan/text/text.h>

struct text_push_data_t
{
    struct kan_float_matrix_4x4_t projection_view;
    struct kan_float_vector_4_t element_offset;
};

static const char *text_shader =
    "push_constant push\n"
    "{\n"
    "    f4x4 projection_view;\n"
    "    f4 element_offset;\n"
    "};\n"
    "\n"
    "vertex_attribute_container vertex\n"
    "{\n"
    "    f2 position;\n"
    "};\n"
    "\n"
    "instanced_attribute_container instanced\n"
    "{\n"
    "    f4 min_max;\n"
    "    f4 uv_min_max;\n"
    "    u1 layer;\n"
    "    u1 mark;\n"
    "};\n"
    "\n"
    "state_container state\n"
    "{\n"
    "    u1 layer;\n"
    "    u1 mark;\n"
    "    f2 uv;\n"
    "};\n"
    "\n"
    "void vertex_main (void)\n"
    "{\n"
    "    state.layer = instanced.layer;\n"
    "    state.mark = instanced.mark;\n"
    "    state.uv = \n"
    "        instanced.uv_min_max.xy + vertex.position * (instanced.uv_min_max.zw - instanced.uv_min_max.xy);\n"
    "    f2 position = instanced.min_max.xy + vertex.position * (instanced.min_max.zw - instanced.min_max.xy);\n"
    "    vertex_stage_output_position (push.projection_view * f4 {push.element_offset.xy + position, 0.5, 1.0});\n"
    "}\n"
    "\n"
    "set_material sampler font_sampler;\n"
    "set_material image_color_2d_array font_atlas;\n"
    "\n"
    "color_output_container fragment_output\n"
    "{\n"
    "    f4 color;\n"
    "};\n"
    "\n"
    "constant smoothing = 1.0 / 32.0;\n"
    "constant bound = 0.5;\n"
    "\n"
    "void fragment_main (void)\n"
    "{\n"
    "    f1 distance = sample (font_sampler, font_atlas, state.layer, state.uv).x;\n"
    "    f1 alpha = smooth_step_f1 (bound - smoothing, bound + smoothing, distance);\n"
    "    if (state.mark == 1u)\n"
    "    {\n"
    "        fragment_output.color = f4 {1.0, 0.0, 0.0, alpha};\n"
    "    }\n"
    "    else\n"
    "    {\n"
    "        fragment_output.color = f4 {1.0, 1.0, 1.0, alpha};\n"
    "    }\n"
    "}\n";

static kan_render_pass_t create_text_pass (kan_render_context_t render_context)
{
    struct kan_render_pass_attachment_t attachments[] = {
        {
            .type = KAN_RENDER_PASS_ATTACHMENT_COLOR,
            .format = KAN_RENDER_IMAGE_FORMAT_RGBA32_SRGB,
            .samples = 1u,
            .load_operation = KAN_RENDER_LOAD_OPERATION_CLEAR,
            .store_operation = KAN_RENDER_STORE_OPERATION_STORE,
        },
    };

    struct kan_render_pass_description_t description = {
        .type = KAN_RENDER_PASS_GRAPHICS,
        .attachments_count = sizeof (attachments) / sizeof (attachments[0u]),
        .attachments = attachments,
        .tracking_name = kan_string_intern ("text"),
    };

    return kan_render_pass_create (render_context, &description);
}

static inline bool emit_render_code (kan_rpl_compiler_instance_t compiler_instance,
                                     struct kan_dynamic_array_t *output,
                                     kan_allocation_group_t output_allocation_group)
{
    const kan_memory_size_t supported_formats = kan_render_get_supported_code_format_flags ();
    if (supported_formats & (1u << KAN_RENDER_CODE_FORMAT_SPIRV))
    {
        return kan_rpl_compiler_instance_emit_spirv (compiler_instance, output, output_allocation_group);
    }

    return false;
}

static kan_render_graphics_pipeline_t create_text_pipeline (
    kan_render_context_t render_context,
    kan_render_pass_t text_pass,
    kan_instance_size_t *output_attribute_vertex_binding,
    kan_instance_size_t *output_instanced_vertex_binding,
    kan_instance_size_t *output_font_sampler_binding,
    kan_instance_size_t *output_font_atlas_binding,
    kan_render_pipeline_parameter_set_layout_t *material_set_layout_output)
{
    kan_rpl_parser_t parser = kan_rpl_parser_create (kan_string_intern ("text"));
    KAN_TEST_ASSERT (kan_rpl_parser_add_source (parser, text_shader, kan_string_intern ("text")))
    struct kan_rpl_intermediate_t intermediate;
    kan_rpl_intermediate_init (&intermediate);
    KAN_TEST_ASSERT (kan_rpl_parser_build_intermediate (parser, &intermediate))
    kan_rpl_parser_destroy (parser);

    kan_rpl_compiler_context_t compiler_context =
        kan_rpl_compiler_context_create (KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC, kan_string_intern ("text"));
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
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (compiler_instance))

    KAN_TEST_ASSERT (emit_render_code (compiler_instance, &code, KAN_ALLOCATION_GROUP_IGNORE))
    KAN_TEST_ASSERT (kan_rpl_compiler_instance_emit_meta (compiler_instance, &meta, KAN_RPL_META_EMISSION_FULL))
    kan_rpl_compiler_instance_destroy (compiler_instance);

    kan_rpl_compiler_context_destroy (compiler_context);
    kan_rpl_intermediate_shutdown (&intermediate);

    struct kan_render_attribute_source_description_t attribute_sources[2u];
    struct kan_render_attribute_description_t attributes[5u];

    struct kan_render_parameter_binding_description_t material_set_bindings[2u];
    struct kan_render_pipeline_parameter_set_layout_description_t material_set_description = {
        .bindings_count = sizeof (material_set_bindings) / sizeof (material_set_bindings[0u]),
        .bindings = material_set_bindings,
        .tracking_name = kan_string_intern ("text_material"),
    };

    KAN_TEST_ASSERT (meta.attribute_sources.size == 2u)
    struct kan_rpl_meta_attribute_source_t *attribute_source =
        &((struct kan_rpl_meta_attribute_source_t *) meta.attribute_sources.data)[0u];
    attribute_sources[0u].binding = attribute_source->binding;
    *output_attribute_vertex_binding = attribute_source->binding;
    attribute_sources[0u].stride = attribute_source->block_size;
    attribute_sources[0u].rate = KAN_RENDER_ATTRIBUTE_RATE_PER_VERTEX;

    KAN_TEST_ASSERT (attribute_source->attributes.size == 1u)
    struct kan_rpl_meta_attribute_t *attribute =
        &((struct kan_rpl_meta_attribute_t *) attribute_source->attributes.data)[0u];

    attributes[0u].binding = attribute_sources->binding;
    attributes[0u].location = attribute->location;
    attributes[0u].offset = attribute->offset;
    KAN_TEST_CHECK (attribute->class == KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_2)
    attributes[0u].class = KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_2;
    KAN_TEST_CHECK (attribute->item_format == KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_32)
    attributes[0u].item_format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_32;

    attribute_source = &((struct kan_rpl_meta_attribute_source_t *) meta.attribute_sources.data)[1u];
    attribute_sources[1u].binding = attribute_source->binding;
    *output_instanced_vertex_binding = attribute_source->binding;
    attribute_sources[1u].stride = attribute_source->block_size;
    attribute_sources[1u].rate = KAN_RENDER_ATTRIBUTE_RATE_PER_INSTANCE;

    KAN_TEST_ASSERT (attribute_source->attributes.size == 4u)
    attribute = &((struct kan_rpl_meta_attribute_t *) attribute_source->attributes.data)[0u];

    attributes[1u].binding = attribute_source->binding;
    attributes[1u].location = attribute->location;
    attributes[1u].offset = attribute->offset;
    KAN_TEST_CHECK (attribute->class == KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_4)
    attributes[1u].class = KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_4;
    KAN_TEST_CHECK (attribute->item_format == KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_32)
    attributes[1u].item_format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_32;

    attribute = &((struct kan_rpl_meta_attribute_t *) attribute_source->attributes.data)[1u];
    attributes[2u].binding = attribute_source->binding;
    attributes[2u].location = attribute->location;
    attributes[2u].offset = attribute->offset;
    KAN_TEST_CHECK (attribute->class == KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_4)
    attributes[2u].class = KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_4;
    KAN_TEST_CHECK (attribute->item_format == KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_32)
    attributes[2u].item_format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_32;

    attribute = &((struct kan_rpl_meta_attribute_t *) attribute_source->attributes.data)[2u];
    attributes[3u].binding = attribute_source->binding;
    attributes[3u].location = attribute->location;
    attributes[3u].offset = attribute->offset;
    KAN_TEST_CHECK (attribute->class == KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_1)
    attributes[3u].class = KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_1;
    KAN_TEST_CHECK (attribute->item_format == KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_32)
    attributes[3u].item_format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_UINT_32;

    attribute = &((struct kan_rpl_meta_attribute_t *) attribute_source->attributes.data)[3u];
    attributes[4u].binding = attribute_source->binding;
    attributes[4u].location = attribute->location;
    attributes[4u].offset = attribute->offset;
    KAN_TEST_CHECK (attribute->class == KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_1)
    attributes[4u].class = KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_1;
    KAN_TEST_CHECK (attribute->item_format == KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_32)
    attributes[4u].item_format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_UINT_32;

    KAN_TEST_ASSERT (meta.set_pass.buffers.size == 0u)
    KAN_TEST_ASSERT (meta.set_material.samplers.size == 1u)
    struct kan_rpl_meta_sampler_t *sampler = &((struct kan_rpl_meta_sampler_t *) meta.set_material.samplers.data)[0u];

    material_set_bindings[0u].binding = sampler->binding;
    *output_font_sampler_binding = sampler->binding;
    material_set_bindings[0u].type = KAN_RENDER_PARAMETER_BINDING_TYPE_SAMPLER;
    material_set_bindings[0u].descriptor_count = 1u;
    material_set_bindings[0u].used_stage_mask =
        (1u << KAN_RENDER_STAGE_GRAPHICS_VERTEX) | (1u << KAN_RENDER_STAGE_GRAPHICS_FRAGMENT);

    KAN_TEST_ASSERT (meta.set_material.images.size == 1u)
    struct kan_rpl_meta_image_t *image = &((struct kan_rpl_meta_image_t *) meta.set_material.images.data)[0u];

    material_set_bindings[1u].binding = image->binding;
    *output_font_atlas_binding = image->binding;
    material_set_bindings[1u].type = KAN_RENDER_PARAMETER_BINDING_TYPE_IMAGE;
    material_set_bindings[1u].descriptor_count = 1u;
    material_set_bindings[1u].used_stage_mask =
        (1u << KAN_RENDER_STAGE_GRAPHICS_VERTEX) | (1u << KAN_RENDER_STAGE_GRAPHICS_FRAGMENT);

    *material_set_layout_output =
        kan_render_pipeline_parameter_set_layout_create (render_context, &material_set_description);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (*material_set_layout_output))

    kan_render_code_module_t code_module = kan_render_code_module_create (render_context, code.size * code.item_size,
                                                                          code.data, kan_string_intern ("cube"));

    struct kan_render_color_output_setup_description_t output_setups[1u] = {
        {
            .use_blend = true,
            .write_r = true,
            .write_g = true,
            .write_b = true,
            .write_a = true,
            .source_color_blend_factor = KAN_RENDER_BLEND_FACTOR_SOURCE_ALPHA,
            .destination_color_blend_factor = KAN_RENDER_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA,
            .color_blend_operation = KAN_RENDER_BLEND_OPERATION_ADD,
            .source_alpha_blend_factor = KAN_RENDER_BLEND_FACTOR_SOURCE_ALPHA,
            .destination_alpha_blend_factor = KAN_RENDER_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA,
            .alpha_blend_operation = KAN_RENDER_BLEND_OPERATION_ADD,
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

    kan_render_pipeline_parameter_set_layout_t set_layouts[] = {
        KAN_HANDLE_INITIALIZE_INVALID,
        *material_set_layout_output,
        KAN_HANDLE_INITIALIZE_INVALID,
        KAN_HANDLE_INITIALIZE_INVALID,
    };

    struct kan_render_graphics_pipeline_description_t pipeline_description = {
        .pass = text_pass,
        .topology = KAN_RENDER_GRAPHICS_TOPOLOGY_TRIANGLE_LIST,
        .polygon_mode = KAN_RENDER_POLYGON_MODE_FILL,
        .cull_mode = KAN_RENDER_CULL_MODE_BACK,
        .use_depth_clamp = false,
        .attribute_sources_count = sizeof (attribute_sources) / (sizeof (attribute_sources[0u])),
        .attribute_sources = attribute_sources,
        .attributes_count = sizeof (attributes) / (sizeof (attributes[0u])),
        .attributes = attributes,
        .push_constant_size = sizeof (struct text_push_data_t),
        .parameter_set_layouts_count = sizeof (set_layouts) / sizeof (set_layouts[0u]),
        .parameter_set_layouts = set_layouts,
        .output_setups_count = sizeof (output_setups) / sizeof (output_setups[0u]),
        .output_setups = output_setups,
        .blend_constant_r = 0.0f,
        .blend_constant_g = 0.0f,
        .blend_constant_b = 0.0f,
        .blend_constant_a = 0.0f,
        .depth_test_enabled = false,
        .depth_write_enabled = false,
        .depth_bounds_test_enabled = false,
        .depth_compare_operation = KAN_RENDER_COMPARE_OPERATION_LESS,
        .min_depth = 0.0f,
        .max_depth = 1.0f,
        .stencil_test_enabled = false,
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
        .tracking_name = kan_string_intern ("text"),
    };

    kan_render_graphics_pipeline_t pipeline = kan_render_graphics_pipeline_create (
        render_context, &pipeline_description, KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CRITICAL);
    kan_render_code_module_destroy (code_module);

    kan_rpl_meta_shutdown (&meta);
    kan_dynamic_array_shutdown (&code);
    return pipeline;
}

#define TEST_WIDTH 800u
#define TEST_HEIGHT 800u

#define FONT_STYLE_NAME_BOLD "bold"

static void check_rgba_equal_enough (uint32_t *first, uint32_t *second, uint32_t count)
{
    uint32_t error_count = 0u;
    // Not more than 1% of errors.
    uint32_t max_error_count = count / 100u;

    while (count--)
    {
        if (*first != *second)
        {
            ++error_count;
        }

        ++first;
        ++second;
    }

    KAN_TEST_CHECK (error_count < max_error_count)
}

static void run_test (const char *expectation_file, struct kan_text_shaping_request_t *text_request)
{
    struct kan_stream_t *input_stream_regular = kan_direct_file_stream_open_for_read (
        "../../../tests_resources/text/fonts/OpenSans-VariableFont_wdth,wght.ttf", true);
    KAN_TEST_ASSERT (input_stream_regular)

    KAN_TEST_ASSERT (input_stream_regular->operations->seek (input_stream_regular, KAN_STREAM_SEEK_END, 0))
    const kan_file_size_t font_file_size_regular = input_stream_regular->operations->tell (input_stream_regular);
    KAN_TEST_ASSERT (input_stream_regular->operations->seek (input_stream_regular, KAN_STREAM_SEEK_START, 0))

    void *font_memory_regular = kan_allocate_general (KAN_ALLOCATION_GROUP_IGNORE, font_file_size_regular, 1u);
    CUSHION_DEFER { kan_free_general (KAN_ALLOCATION_GROUP_IGNORE, font_memory_regular, font_file_size_regular); }

    const kan_file_size_t font_read_regular =
        input_stream_regular->operations->read (input_stream_regular, font_file_size_regular, font_memory_regular);
    KAN_TEST_ASSERT (font_read_regular == font_file_size_regular)
    input_stream_regular->operations->close (input_stream_regular);

    struct kan_stream_t *input_stream_italic = kan_direct_file_stream_open_for_read (
        "../../../tests_resources/text/fonts/OpenSans-Italic-VariableFont_wdth,wght.ttf", true);
    KAN_TEST_ASSERT (input_stream_italic)

    KAN_TEST_ASSERT (input_stream_italic->operations->seek (input_stream_italic, KAN_STREAM_SEEK_END, 0))
    const kan_file_size_t font_file_size_italic = input_stream_italic->operations->tell (input_stream_italic);
    KAN_TEST_ASSERT (input_stream_italic->operations->seek (input_stream_italic, KAN_STREAM_SEEK_START, 0))

    void *font_memory_italic = kan_allocate_general (KAN_ALLOCATION_GROUP_IGNORE, font_file_size_italic, 1u);
    CUSHION_DEFER { kan_free_general (KAN_ALLOCATION_GROUP_IGNORE, font_memory_italic, font_file_size_italic); }

    const kan_file_size_t font_read_italic =
        input_stream_italic->operations->read (input_stream_italic, font_file_size_italic, font_memory_italic);
    KAN_TEST_ASSERT (font_read_italic == font_file_size_italic)
    input_stream_italic->operations->close (input_stream_italic);

    struct kan_stream_t *input_stream_arabic = kan_direct_file_stream_open_for_read (
        "../../../tests_resources/text/fonts/Cairo-VariableFont_slnt,wght.ttf", true);
    KAN_TEST_ASSERT (input_stream_arabic)

    KAN_TEST_ASSERT (input_stream_arabic->operations->seek (input_stream_arabic, KAN_STREAM_SEEK_END, 0))
    const kan_file_size_t font_file_size_arabic = input_stream_arabic->operations->tell (input_stream_arabic);
    KAN_TEST_ASSERT (input_stream_arabic->operations->seek (input_stream_arabic, KAN_STREAM_SEEK_START, 0))

    void *font_memory_arabic = kan_allocate_general (KAN_ALLOCATION_GROUP_IGNORE, font_file_size_arabic, 1u);
    CUSHION_DEFER { kan_free_general (KAN_ALLOCATION_GROUP_IGNORE, font_memory_arabic, font_file_size_arabic); }

    const kan_file_size_t font_read_arabic =
        input_stream_arabic->operations->read (input_stream_arabic, font_file_size_arabic, font_memory_arabic);
    KAN_TEST_ASSERT (font_read_arabic == font_file_size_arabic)
    input_stream_arabic->operations->close (input_stream_arabic);

    kan_platform_application_init ();
    CUSHION_DEFER { kan_platform_application_shutdown (); }

    kan_context_t context =
        kan_context_create (kan_allocation_group_get_child (kan_allocation_group_root (), "context"));
    CUSHION_DEFER { kan_context_destroy (context); }

    struct kan_render_backend_system_config_t render_backend_config = {
        .application_info_name = kan_string_intern ("Kan autotest"),
        .version_major = 1u,
        .version_minor = 0u,
        .version_patch = 0u,
    };

    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (
        kan_context_request_system (context, KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME, &render_backend_config))

    kan_context_assembly (context);
    kan_context_system_t application_system = kan_context_query (context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);
    CUSHION_DEFER { kan_application_system_prepare_for_destroy_in_main_thread (application_system); }

    kan_context_system_t render_backend_system = kan_context_query (context, KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME);
    kan_render_context_t render_context = kan_render_backend_system_get_render_context (render_backend_system);

    struct kan_render_supported_devices_t *devices = kan_render_backend_system_get_devices (render_backend_system);
    printf ("Devices (%lu):\n", (unsigned long) devices->supported_device_count);
    kan_render_device_t picked_device = KAN_HANDLE_INITIALIZE_INVALID;
    kan_instance_size_t picked_device_index = KAN_INT_MAX (kan_instance_size_t);

    for (kan_loop_size_t index = 0u; index < devices->supported_device_count; ++index)
    {
        printf ("  - name: %s\n    device_type: %lu\n    memory_type: %lu\n", devices->devices[index].name,
                (unsigned long) devices->devices[index].device_type,
                (unsigned long) devices->devices[index].memory_type);

        if (picked_device_index == KAN_INT_MAX (kan_instance_size_t) ||
            devices->devices[picked_device_index].device_type != KAN_RENDER_DEVICE_TYPE_DISCRETE_GPU)
        {
            picked_device = devices->devices[index].id;
            picked_device_index = index;
        }
    }

    kan_render_backend_system_select_device (render_backend_system, picked_device);
    float open_sans_regular_variable_axis[] = {
        400.0f,
        100.0f,
    };

    float open_sans_bold_variable_axis[] = {
        700.0f,
        100.0f,
    };

    float cairo_regular_variable_axis[] = {
        400.0f,
        5.0f,
    };

    float cairo_bold_variable_axis[] = {
        700.0f,
        5.0f,
    };

    struct kan_font_library_category_t font_library_categories[] = {
        {
            .script = kan_string_intern ("Latn"),
            .style = NULL,
            .variable_axis_count =
                sizeof (open_sans_regular_variable_axis) / sizeof (open_sans_regular_variable_axis[0u]),
            .variable_axis = open_sans_regular_variable_axis,
            .data_size = (kan_memory_size_t) font_file_size_regular,
            .data = font_memory_regular,
        },
        {
            .script = kan_string_intern ("Latn"),
            .style = kan_string_intern (FONT_STYLE_NAME_BOLD),
            .variable_axis_count = sizeof (open_sans_bold_variable_axis) / sizeof (open_sans_bold_variable_axis[0u]),
            .variable_axis = open_sans_bold_variable_axis,
            .data_size = (kan_memory_size_t) font_file_size_regular,
            .data = font_memory_regular,
        },
        {
            .script = kan_string_intern ("Cyrl"),
            .style = NULL,
            .variable_axis_count =
                sizeof (open_sans_regular_variable_axis) / sizeof (open_sans_regular_variable_axis[0u]),
            .variable_axis = open_sans_regular_variable_axis,
            .data_size = (kan_memory_size_t) font_file_size_italic,
            .data = font_memory_italic,
        },
        {
            .script = kan_string_intern ("Cyrl"),
            .style = kan_string_intern (FONT_STYLE_NAME_BOLD),
            .variable_axis_count = sizeof (open_sans_bold_variable_axis) / sizeof (open_sans_bold_variable_axis[0u]),
            .variable_axis = open_sans_bold_variable_axis,
            .data_size = (kan_memory_size_t) font_file_size_italic,
            .data = font_memory_italic,
        },
        {
            .script = kan_string_intern ("Arab"),
            .style = NULL,
            .variable_axis_count = sizeof (cairo_regular_variable_axis) / sizeof (cairo_regular_variable_axis[0u]),
            .variable_axis = cairo_regular_variable_axis,
            .data_size = (kan_memory_size_t) font_file_size_arabic,
            .data = font_memory_arabic,
        },
        {
            .script = kan_string_intern ("Arab"),
            .style = kan_string_intern (FONT_STYLE_NAME_BOLD),
            .variable_axis_count = sizeof (cairo_bold_variable_axis) / sizeof (cairo_bold_variable_axis[0u]),
            .variable_axis = cairo_bold_variable_axis,
            .data_size = (kan_memory_size_t) font_file_size_arabic,
            .data = font_memory_arabic,
        },
    };

    kan_font_library_t font_library = kan_font_library_create (
        render_context, sizeof (font_library_categories) / sizeof (font_library_categories[0u]),
        font_library_categories);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (font_library))
    CUSHION_DEFER { kan_font_library_destroy (font_library); }

    // Precache mostly to check that it doesn't break anything.
    struct kan_text_precache_request_t precache_request = {
        .script = kan_string_intern ("Latn"),
        .style = NULL,
        .render_format = KAN_FONT_GLYPH_RENDER_FORMAT_SDF,
        .orientation = KAN_TEXT_ORIENTATION_HORIZONTAL,
        .utf8 = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",
    };

    KAN_TEST_ASSERT (kan_font_library_precache (font_library, &precache_request))
    struct kan_text_shaped_data_t shaped_data;
    kan_text_shaped_data_init (&shaped_data);
    CUSHION_DEFER { kan_text_shaped_data_shutdown (&shaped_data); }
    KAN_TEST_ASSERT (kan_font_library_shape (font_library, text_request, &shaped_data))

    kan_render_pass_t text_pass = create_text_pass (render_context);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (text_pass))

    kan_instance_size_t text_attribute_vertex_binding;
    kan_instance_size_t text_instanced_vertex_binding;
    kan_instance_size_t text_font_sampler_binding;
    kan_instance_size_t text_font_atlas_binding;
    kan_render_pipeline_parameter_set_layout_t text_material_set_layout;

    kan_render_graphics_pipeline_t text_pipeline =
        create_text_pipeline (render_context, text_pass, &text_attribute_vertex_binding, &text_instanced_vertex_binding,
                              &text_font_sampler_binding, &text_font_atlas_binding, &text_material_set_layout);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (text_pipeline))

    struct kan_render_image_description_t scene_target_image_description = {
        .format = KAN_RENDER_IMAGE_FORMAT_RGBA32_SRGB,
        .width = TEST_WIDTH,
        .height = TEST_HEIGHT,
        .depth = 1u,
        .layers = 1u,
        .mips = 1u,
        .render_target = true,
        .supports_sampling = false,
        .always_treat_as_layered = false,
        .tracking_name = kan_string_intern ("render_target"),
    };

    kan_render_image_t scene_render_target_image =
        kan_render_image_create (render_context, &scene_target_image_description);

    struct kan_render_frame_buffer_attachment_description_t surface_frame_buffer_attachments[] = {
        {
            .image = scene_render_target_image,
            .layer = 0u,
        },
    };

    struct kan_render_frame_buffer_description_t surface_frame_buffer_description = {
        .associated_pass = text_pass,
        .attachments_count = sizeof (surface_frame_buffer_attachments) / sizeof (surface_frame_buffer_attachments[0u]),
        .attachments = surface_frame_buffer_attachments,
        .tracking_name = kan_string_intern ("surface"),
    };

    kan_render_frame_buffer_t surface_frame_buffer =
        kan_render_frame_buffer_create (render_context, &surface_frame_buffer_description);

    struct kan_float_vector_2_t text_vertices[] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f},
    };

    kan_render_buffer_t text_vertex_buffer =
        kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_ATTRIBUTE, sizeof (text_vertices),
                                  text_vertices, kan_string_intern ("text"));

    uint16_t text_indices[] = {
        0u, 1u, 2u, 2u, 3u, 0u,
    };

    kan_render_buffer_t text_index_buffer =
        kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_INDEX_16, sizeof (text_indices), text_indices,
                                  kan_string_intern ("text"));

    kan_render_frame_lifetime_buffer_allocator_t frame_lifetime_allocator =
        kan_render_frame_lifetime_buffer_allocator_create (render_context, KAN_RENDER_BUFFER_TYPE_ATTRIBUTE,
                                                           1024u * 1024u, false,
                                                           kan_string_intern ("instanced_attributes"));

    struct kan_render_parameter_update_description_t text_material_parameters[] = {
        {
            .binding = text_font_sampler_binding,
            .sampler_binding =
                {
                    .sampler =
                        {
                            .mag_filter = KAN_RENDER_FILTER_MODE_LINEAR,
                            .min_filter = KAN_RENDER_FILTER_MODE_LINEAR,
                            .mip_map_mode = KAN_RENDER_MIP_MAP_MODE_NEAREST,
                            .address_mode_u = KAN_RENDER_ADDRESS_MODE_CLAMP_TO_EDGE,
                            .address_mode_v = KAN_RENDER_ADDRESS_MODE_CLAMP_TO_EDGE,
                            .address_mode_w = KAN_RENDER_ADDRESS_MODE_CLAMP_TO_EDGE,
                        },
                },
        },
        {
            .binding = text_font_atlas_binding,
            .image_binding =
                {
                    // For the sake of test simplicity, we do not expect sdf atlas reallocation here.
                    .image = kan_font_library_get_sdf_atlas (font_library),
                    .array_index = 0u,
                    .layer_offset = 0u,
                    .layer_count = 1u,
                },
        },
    };

    struct kan_render_pipeline_parameter_set_description_t text_material_set_description = {
        .layout = text_material_set_layout,
        .stable_binding = true,
        .tracking_name = kan_string_intern ("text_material"),
        .initial_bindings_count = sizeof (text_material_parameters) / sizeof (text_material_parameters[0u]),
        .initial_bindings = text_material_parameters,
    };

    kan_render_pipeline_parameter_set_t material_text_set =
        kan_render_pipeline_parameter_set_create (render_context, &text_material_set_description);

    kan_render_read_back_status_t frame_read_back = KAN_HANDLE_SET_INVALID (kan_render_read_back_status_t);
    kan_render_buffer_t read_back_buffer =
        kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE,
                                  TEST_WIDTH * TEST_HEIGHT * 4u, NULL, kan_string_intern ("read_back"));

    while (!KAN_HANDLE_IS_VALID (frame_read_back) ||
           kan_read_read_back_status_get (frame_read_back) != KAN_RENDER_READ_BACK_STATE_FINISHED)
    {
        kan_application_system_sync_in_main_thread (application_system);
        if (!kan_render_backend_system_next_frame (render_backend_system) || KAN_HANDLE_IS_VALID (frame_read_back))
        {
            continue;
        }

        struct kan_render_viewport_bounds_t text_viewport_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float) TEST_WIDTH,
            .height = (float) TEST_HEIGHT,
            .depth_min = 0.0f,
            .depth_max = 1.0f,
        };

        struct kan_render_integer_region_2d_t text_scissor = {
            .x = 0,
            .y = 0,
            .width = TEST_WIDTH,
            .height = TEST_HEIGHT,
        };

        struct kan_render_clear_value_t text_attachment_clear_values[] = {
            {
                .color = {0.0f, 0.0f, 0.0f, 1.0f},
            },
            {
                .depth_stencil = {1.0f, 0u},
            },
        };

        kan_render_pass_instance_t text_instance = kan_render_pass_instantiate (
            text_pass, surface_frame_buffer, &text_viewport_bounds, &text_scissor, text_attachment_clear_values);

        if (!KAN_HANDLE_IS_VALID (text_instance) ||
            !kan_render_pass_instance_graphics_pipeline (text_instance, text_pipeline))
        {
            continue;
        }

        kan_render_pass_instance_pipeline_parameter_sets (text_instance, KAN_RPL_SET_MATERIAL, 1u, &material_text_set);

        struct kan_render_allocated_slice_t slice = kan_render_frame_lifetime_buffer_allocator_allocate (
            frame_lifetime_allocator, sizeof (struct kan_text_shaped_glyph_instance_data_t) * shaped_data.glyphs.size,
            alignof (struct kan_text_shaped_glyph_instance_data_t));

        void *text_instanced_memory =
            kan_render_buffer_patch (slice.buffer, slice.slice_offset,
                                     sizeof (struct kan_text_shaped_glyph_instance_data_t) * shaped_data.glyphs.size);
        KAN_TEST_ASSERT (text_instanced_memory)

        memcpy (text_instanced_memory, shaped_data.glyphs.data,
                sizeof (struct kan_text_shaped_glyph_instance_data_t) * shaped_data.glyphs.size);

        kan_render_pass_instance_indices (text_instance, text_index_buffer);
        kan_render_pass_instance_attributes (text_instance, (kan_instance_size_t) text_attribute_vertex_binding, 1u,
                                             &text_vertex_buffer, NULL);

        kan_render_pass_instance_attributes (text_instance, (kan_instance_size_t) text_instanced_vertex_binding, 1u,
                                             &slice.buffer, &slice.slice_offset);

        struct text_push_data_t push_data;
        push_data.projection_view =
            kan_orthographic_projection (0.0f, (float) TEST_WIDTH, (float) TEST_HEIGHT, 0.0f, 0.01f, 5000.0f);

        push_data.element_offset.x = 100.0f - (float) shaped_data.min.x;
        push_data.element_offset.y = 100.0f - (float) shaped_data.min.y;
        push_data.element_offset.z = 0.0f;
        push_data.element_offset.w = 0.0f;
        kan_render_pass_instance_push_constant (text_instance, &push_data);

        kan_render_pass_instance_draw (text_instance, 0u, sizeof (text_indices) / sizeof (text_indices[0u]), 0u, 0u,
                                       shaped_data.glyphs.size);

        frame_read_back = kan_render_request_read_back_from_image (scene_render_target_image, 0u, 0u, read_back_buffer,
                                                                   0u, text_instance);
        KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (frame_read_back))
    }

    struct kan_image_raw_data_t frame_raw_data;
    frame_raw_data.width = (kan_instance_size_t) TEST_WIDTH;
    frame_raw_data.height = (kan_instance_size_t) TEST_HEIGHT;

    frame_raw_data.data = (uint8_t *) kan_render_buffer_read (read_back_buffer);
    KAN_TEST_ASSERT (frame_raw_data.data)

    {
        struct kan_stream_t *output_stream = kan_direct_file_stream_open_for_write ("result.png", true);
        KAN_TEST_ASSERT (output_stream)
        KAN_TEST_ASSERT (kan_image_save (output_stream, KAN_IMAGE_SAVE_FORMAT_PNG, &frame_raw_data));
        output_stream->operations->close (output_stream);
    }

    struct kan_image_raw_data_t expected_raw_data;
    kan_image_raw_data_init (&expected_raw_data);
    CUSHION_DEFER { kan_image_raw_data_shutdown (&expected_raw_data); }

    {
        struct kan_stream_t *input_stream = kan_direct_file_stream_open_for_read (expectation_file, true);
        KAN_TEST_ASSERT (input_stream)
        KAN_TEST_ASSERT (kan_image_load (input_stream, &expected_raw_data))
        input_stream->operations->close (input_stream);
    }

    check_rgba_equal_enough ((uint32_t *) frame_raw_data.data, (uint32_t *) expected_raw_data.data,
                             TEST_WIDTH * TEST_HEIGHT);
}

KAN_TEST_CASE (english_left)
{
    struct kan_text_item_t text_content[] = {
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 = "Robert Guiscard also referred to as Robert de Hauteville, was a Norman adventurer remembered for "
                    "his conquest of southern Italy and Sicily in the 11th century.\n"
                    "\n"
                    "Robert was born into the Hauteville family in Normandy, the sixth son of Tancred de Hauteville "
                    "and his wife Fressenda. He inherited the County of Apulia and Calabria from his brother in 1057, "
                    "and in 1059 he was made Duke of Apulia and Calabria and Lord of Sicily by Pope Nicholas II. He "
                    "was also briefly Prince of Benevento (1078–1081), before returning the title to the papacy.\n",
        },
    };

    kan_text_t text = kan_text_create (sizeof (text_content) / sizeof (text_content[0u]), text_content);
    CUSHION_DEFER { kan_text_destroy (text); }

    struct kan_text_shaped_data_t shaped_data;
    kan_text_shaped_data_init (&shaped_data);
    CUSHION_DEFER { kan_text_shaped_data_shutdown (&shaped_data); }

    struct kan_text_shaping_request_t request = {
        .font_size = 30u,
        .render_format = KAN_FONT_GLYPH_RENDER_FORMAT_SDF,
        .orientation = KAN_TEXT_ORIENTATION_HORIZONTAL,
        .reading_direction = KAN_TEXT_READING_DIRECTION_LEFT_TO_RIGHT,
        .alignment = KAN_TEXT_SHAPING_ALIGNMENT_LEFT,
        .primary_axis_limit = 600u,
        .text = text,
    };

    run_test ("../../../tests_resources/text/expectations/english_left.png", &request);
}

KAN_TEST_CASE (english_center)
{
    struct kan_text_item_t text_content[] = {
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 = "Robert Guiscard also referred to as Robert de Hauteville, was a Norman adventurer remembered for "
                    "his conquest of southern Italy and Sicily in the 11th century.\n"
                    "\n"
                    "Robert was born into the Hauteville family in Normandy, the sixth son of Tancred de Hauteville "
                    "and his wife Fressenda. He inherited the County of Apulia and Calabria from his brother in 1057, "
                    "and in 1059 he was made Duke of Apulia and Calabria and Lord of Sicily by Pope Nicholas II. He "
                    "was also briefly Prince of Benevento (1078–1081), before returning the title to the papacy.\n",
        },
    };

    kan_text_t text = kan_text_create (sizeof (text_content) / sizeof (text_content[0u]), text_content);
    CUSHION_DEFER { kan_text_destroy (text); }

    struct kan_text_shaped_data_t shaped_data;
    kan_text_shaped_data_init (&shaped_data);
    CUSHION_DEFER { kan_text_shaped_data_shutdown (&shaped_data); }

    struct kan_text_shaping_request_t request = {
        .font_size = 30u,
        .render_format = KAN_FONT_GLYPH_RENDER_FORMAT_SDF,
        .orientation = KAN_TEXT_ORIENTATION_HORIZONTAL,
        .reading_direction = KAN_TEXT_READING_DIRECTION_LEFT_TO_RIGHT,
        .alignment = KAN_TEXT_SHAPING_ALIGNMENT_CENTER,
        .primary_axis_limit = 600u,
        .text = text,
    };

    run_test ("../../../tests_resources/text/expectations/english_center.png", &request);
}

KAN_TEST_CASE (english_right)
{
    struct kan_text_item_t text_content[] = {
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 = "Robert Guiscard also referred to as Robert de Hauteville, was a Norman adventurer remembered for "
                    "his conquest of southern Italy and Sicily in the 11th century.\n"
                    "\n"
                    "Robert was born into the Hauteville family in Normandy, the sixth son of Tancred de Hauteville "
                    "and his wife Fressenda. He inherited the County of Apulia and Calabria from his brother in 1057, "
                    "and in 1059 he was made Duke of Apulia and Calabria and Lord of Sicily by Pope Nicholas II. He "
                    "was also briefly Prince of Benevento (1078–1081), before returning the title to the papacy.\n",
        },
    };

    kan_text_t text = kan_text_create (sizeof (text_content) / sizeof (text_content[0u]), text_content);
    CUSHION_DEFER { kan_text_destroy (text); }

    struct kan_text_shaped_data_t shaped_data;
    kan_text_shaped_data_init (&shaped_data);
    CUSHION_DEFER { kan_text_shaped_data_shutdown (&shaped_data); }

    struct kan_text_shaping_request_t request = {
        .font_size = 30u,
        .render_format = KAN_FONT_GLYPH_RENDER_FORMAT_SDF,
        .orientation = KAN_TEXT_ORIENTATION_HORIZONTAL,
        .reading_direction = KAN_TEXT_READING_DIRECTION_LEFT_TO_RIGHT,
        .alignment = KAN_TEXT_SHAPING_ALIGNMENT_RIGHT,
        .primary_axis_limit = 600u,
        .text = text,
    };

    run_test ("../../../tests_resources/text/expectations/english_right.png", &request);
}

KAN_TEST_CASE (english_vertical)
{
    struct kan_text_item_t text_content[] = {
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 = "Robert Guiscard",
        },
    };

    kan_text_t text = kan_text_create (sizeof (text_content) / sizeof (text_content[0u]), text_content);
    CUSHION_DEFER { kan_text_destroy (text); }

    struct kan_text_shaped_data_t shaped_data;
    kan_text_shaped_data_init (&shaped_data);
    CUSHION_DEFER { kan_text_shaped_data_shutdown (&shaped_data); }

    struct kan_text_shaping_request_t request = {
        .font_size = 55u,
        .render_format = KAN_FONT_GLYPH_RENDER_FORMAT_SDF,
        .orientation = KAN_TEXT_ORIENTATION_VERTICAL,
        .reading_direction = KAN_TEXT_READING_DIRECTION_LEFT_TO_RIGHT,
        .alignment = KAN_TEXT_SHAPING_ALIGNMENT_LEFT,
        .primary_axis_limit = 600u,
        .text = text,
    };

    run_test ("../../../tests_resources/text/expectations/english_vertical.png", &request);
}

KAN_TEST_CASE (english_styles)
{
    struct kan_text_item_t text_content[] = {
        {
            .type = KAN_TEXT_ITEM_STYLE,
            .style =
                {
                    .style = kan_string_intern (FONT_STYLE_NAME_BOLD),
                    .mark_index = 1u,
                },
        },
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 = "Robert Guiscard",
        },
        {
            .type = KAN_TEXT_ITEM_STYLE,
            .style =
                {
                    .style = NULL,
                    .mark_index = 0u,
                },
        },
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 = " also referred to as ",
        },
        {
            .type = KAN_TEXT_ITEM_STYLE,
            .style =
                {
                    .style = kan_string_intern (FONT_STYLE_NAME_BOLD),
                    .mark_index = 1u,
                },
        },
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 = "Robert de Hauteville",
        },
        {
            .type = KAN_TEXT_ITEM_STYLE,
            .style =
                {
                    .style = NULL,
                    .mark_index = 0u,
                },
        },
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 = ", was a Norman adventurer remembered for "
                    "his conquest of southern Italy and Sicily in the 11th century.\n"
                    "\n"
                    "Robert was born into the Hauteville family in Normandy, the sixth son of Tancred de Hauteville "
                    "and his wife Fressenda. He inherited the County of Apulia and Calabria from his brother in 1057, "
                    "and in 1059 he was made Duke of Apulia and Calabria and Lord of Sicily by Pope Nicholas II. He "
                    "was also briefly Prince of Benevento (1078–1081), before returning the title to the papacy.\n",
        },
    };

    kan_text_t text = kan_text_create (sizeof (text_content) / sizeof (text_content[0u]), text_content);
    CUSHION_DEFER { kan_text_destroy (text); }

    struct kan_text_shaped_data_t shaped_data;
    kan_text_shaped_data_init (&shaped_data);
    CUSHION_DEFER { kan_text_shaped_data_shutdown (&shaped_data); }

    struct kan_text_shaping_request_t request = {
        .font_size = 30u,
        .render_format = KAN_FONT_GLYPH_RENDER_FORMAT_SDF,
        .orientation = KAN_TEXT_ORIENTATION_HORIZONTAL,
        .reading_direction = KAN_TEXT_READING_DIRECTION_LEFT_TO_RIGHT,
        .alignment = KAN_TEXT_SHAPING_ALIGNMENT_LEFT,
        .primary_axis_limit = 600u,
        .text = text,
    };

    run_test ("../../../tests_resources/text/expectations/english_styles.png", &request);
}

KAN_TEST_CASE (russian)
{
    struct kan_text_item_t text_content[] = {
        {
            .type = KAN_TEXT_ITEM_STYLE,
            .style =
                {
                    .style = kan_string_intern (FONT_STYLE_NAME_BOLD),
                    .mark_index = 1u,
                },
        },
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 = "Роберт Отви́ль",
        },
        {
            .type = KAN_TEXT_ITEM_STYLE,
            .style =
                {
                    .style = NULL,
                    .mark_index = 0u,
                },
        },
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 = " по прозвищу ",
        },
        {
            .type = KAN_TEXT_ITEM_STYLE,
            .style =
                {
                    .style = kan_string_intern (FONT_STYLE_NAME_BOLD),
                    .mark_index = 1u,
                },
        },
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 = "Гвиска́р",
        },
        {
            .type = KAN_TEXT_ITEM_STYLE,
            .style =
                {
                    .style = NULL,
                    .mark_index = 0u,
                },
        },
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 =
                " (старофр. Robert Viscart, лат. Robertus de Altavilla, Robertus cognomento Guiscardus o Viscardus;"
                " 1016, Hauteville-la-Guichard[вд], Королевство Франция — 17 июля 1085, Кефалиния, Византия) — "
                "четвёртый граф (с 1057 года) и первый герцог Апулии (1059—1085) из дома Отвилей.\n"
                "\n"
                "Окончательно изгнал из Италии византийцев (1071), захватил княжество Салерно (1077) и, тем самым, "
                "завершил завоевание нормандцами Южной Италии. Совместно с младшим братом Рожером I начал завоевание "
                "Сицилии (1061). Оказывая помощь папе Григорию VII, овладел Римом и сжёг город (1084). В конце жизни "
                "предпринял попытку завоевать Византию. Был прозван Гвискаром, что переводится со старофранцузского "
                "как «Хитрец».",
        },
    };

    kan_text_t text = kan_text_create (sizeof (text_content) / sizeof (text_content[0u]), text_content);
    CUSHION_DEFER { kan_text_destroy (text); }

    struct kan_text_shaped_data_t shaped_data;
    kan_text_shaped_data_init (&shaped_data);
    CUSHION_DEFER { kan_text_shaped_data_shutdown (&shaped_data); }

    struct kan_text_shaping_request_t request = {
        .font_size = 24u,
        .render_format = KAN_FONT_GLYPH_RENDER_FORMAT_SDF,
        .orientation = KAN_TEXT_ORIENTATION_HORIZONTAL,
        .reading_direction = KAN_TEXT_READING_DIRECTION_LEFT_TO_RIGHT,
        .alignment = KAN_TEXT_SHAPING_ALIGNMENT_LEFT,
        .primary_axis_limit = 600u,
        .text = text,
    };

    run_test ("../../../tests_resources/text/expectations/russian.png", &request);
}

KAN_TEST_CASE (persian)
{
    struct kan_text_item_t text_content[] = {
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 = "اگر به روزی دردی رسید به تو\n"
                    "دگرگونی ز دست تو بر می\u200Cآید\n",
        },
    };

    kan_text_t text = kan_text_create (sizeof (text_content) / sizeof (text_content[0u]), text_content);
    CUSHION_DEFER { kan_text_destroy (text); }

    struct kan_text_shaped_data_t shaped_data;
    kan_text_shaped_data_init (&shaped_data);
    CUSHION_DEFER { kan_text_shaped_data_shutdown (&shaped_data); }

    struct kan_text_shaping_request_t request = {
        .font_size = 60u,
        // Currently, persian glyphs look a little bit of with SDF.
        // I think it is because they are more complex than european and therefore SDF is not behaving well with them.
        .render_format = KAN_FONT_GLYPH_RENDER_FORMAT_SDF,
        .orientation = KAN_TEXT_ORIENTATION_HORIZONTAL,
        .reading_direction = KAN_TEXT_READING_DIRECTION_RIGHT_TO_LEFT,
        .alignment = KAN_TEXT_SHAPING_ALIGNMENT_RIGHT,
        .primary_axis_limit = 600u,
        .text = text,
    };

    run_test ("../../../tests_resources/text/expectations/persian.png", &request);
}

KAN_TEST_CASE (ltr_inside_rtl)
{
    struct kan_text_item_t text_content[] = {
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 = "اگر به روزی دردی رسید به تو"
                    " \u0091test me\u0091 "
                    "دگرگونی ز دست تو بر می\u200Cآید\n",
        },
    };

    kan_text_t text = kan_text_create (sizeof (text_content) / sizeof (text_content[0u]), text_content);
    CUSHION_DEFER { kan_text_destroy (text); }

    struct kan_text_shaped_data_t shaped_data;
    kan_text_shaped_data_init (&shaped_data);
    CUSHION_DEFER { kan_text_shaped_data_shutdown (&shaped_data); }

    struct kan_text_shaping_request_t request = {
        .font_size = 60u,
        .render_format = KAN_FONT_GLYPH_RENDER_FORMAT_SDF,
        .orientation = KAN_TEXT_ORIENTATION_HORIZONTAL,
        .reading_direction = KAN_TEXT_READING_DIRECTION_RIGHT_TO_LEFT,
        .alignment = KAN_TEXT_SHAPING_ALIGNMENT_RIGHT,
        .primary_axis_limit = 600u,
        .text = text,
    };

    run_test ("../../../tests_resources/text/expectations/ltr_inside_rtl.png", &request);
}

KAN_TEST_CASE (ltr_3_langs)
{
    struct kan_text_item_t text_content[] = {
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 = "дратуте! ",
        },
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 = "\u0091"
                    "نوع مفتوح"
                    "\u0091",
        },
        {
            .type = KAN_TEXT_ITEM_UTF8,
            .utf8 = "\nkan_font_library_t font_library = kan_font_library_create (\n&font_library_description);\n;",
        },
    };

    kan_text_t text = kan_text_create (sizeof (text_content) / sizeof (text_content[0u]), text_content);
    CUSHION_DEFER { kan_text_destroy (text); }

    struct kan_text_shaped_data_t shaped_data;
    kan_text_shaped_data_init (&shaped_data);
    CUSHION_DEFER { kan_text_shaped_data_shutdown (&shaped_data); }

    struct kan_text_shaping_request_t request = {
        .font_size = 42u,
        .render_format = KAN_FONT_GLYPH_RENDER_FORMAT_SDF,
        .orientation = KAN_TEXT_ORIENTATION_HORIZONTAL,
        .reading_direction = KAN_TEXT_READING_DIRECTION_LEFT_TO_RIGHT,
        .alignment = KAN_TEXT_SHAPING_ALIGNMENT_CENTER,
        .primary_axis_limit = 600u,
        .text = text,
    };

    run_test ("../../../tests_resources/text/expectations/ltr_3_langs.png", &request);
}
