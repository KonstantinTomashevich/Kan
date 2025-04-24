#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <kan/file_system/stream.h>
#include <kan/render_pipeline_language/compiler.h>
#include <kan/render_pipeline_language/parser.h>
#include <kan/testing/testing.h>

// Disclaimer: tests below are not real bullet-proof tests that validate everything and make sure that there is no bugs.
//             It is only possible to properly validate render code generation when actual render backend is ready
//             and functioning. It is impossible to test such things without coupling them to render backends.
//             Therefore, here we just check that we're able to compile simple scenarios and produce something that
//             can be valid bytecode if we're lucky.

#define PIPELINE_BASE_PATH "../../../tests_resources/render_pipeline_language/"

static void load_pipeline_source (const char *path, struct kan_dynamic_array_t *output)
{
    struct kan_stream_t *file_stream = kan_direct_file_stream_open_for_read (path, KAN_TRUE);
    KAN_TEST_ASSERT (file_stream)

    KAN_TEST_ASSERT (file_stream->operations->seek (file_stream, KAN_STREAM_SEEK_END, 0))
    kan_file_size_t size = file_stream->operations->tell (file_stream);
    KAN_TEST_ASSERT (file_stream->operations->seek (file_stream, KAN_STREAM_SEEK_START, 0))

    kan_dynamic_array_init (output, (kan_instance_size_t) (size + 1u), sizeof (char), _Alignof (char),
                            KAN_ALLOCATION_GROUP_IGNORE);
    output->size = (kan_instance_size_t) (size + 1u);
    KAN_TEST_ASSERT (file_stream->operations->read (file_stream, size, output->data) == size)
    ((char *) output->data)[size] = '\0';
    file_stream->operations->close (file_stream);
}

static void compile_pipeline (kan_rpl_compiler_context_t compiler_context, struct kan_dynamic_array_t *output)
{
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

    kan_rpl_compiler_instance_t code_instance = kan_rpl_compiler_context_resolve (
        compiler_context, sizeof (entry_points) / sizeof (entry_points[0u]), entry_points);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (code_instance))

    KAN_TEST_ASSERT (kan_rpl_compiler_instance_emit_spirv (code_instance, output, KAN_ALLOCATION_GROUP_IGNORE))
    kan_rpl_compiler_instance_destroy (code_instance);
}

static void save_code (const char *path, struct kan_dynamic_array_t *output)
{
    struct kan_stream_t *file_stream = kan_direct_file_stream_open_for_write (path, KAN_TRUE);
    KAN_TEST_ASSERT (file_stream)
    KAN_TEST_ASSERT (file_stream->operations->write (file_stream, output->size * output->item_size, output->data) ==
                     output->size * output->item_size)
    file_stream->operations->close (file_stream);
}

KAN_TEST_CASE (generic)
{
    struct kan_dynamic_array_t library_source;
    load_pipeline_source (PIPELINE_BASE_PATH "generic_library.rpl", &library_source);

    struct kan_dynamic_array_t pipeline_source;
    load_pipeline_source (PIPELINE_BASE_PATH "generic_pipeline.rpl", &pipeline_source);

    kan_rpl_parser_t parser = kan_rpl_parser_create (kan_string_intern ("test"));
    KAN_TEST_CHECK (
        kan_rpl_parser_add_source (parser, (const char *) pipeline_source.data, kan_string_intern ("pipeline")))
    KAN_TEST_CHECK (
        kan_rpl_parser_add_source (parser, (const char *) library_source.data, kan_string_intern ("library")))

    struct kan_rpl_intermediate_t intermediate;
    kan_rpl_intermediate_init (&intermediate);
    KAN_TEST_CHECK (kan_rpl_parser_build_intermediate (parser, &intermediate))
    kan_rpl_parser_destroy (parser);

    kan_dynamic_array_shutdown (&library_source);
    kan_dynamic_array_shutdown (&pipeline_source);

    kan_rpl_compiler_context_t compiler_context =
        kan_rpl_compiler_context_create (KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC, kan_string_intern ("variant_test"));

    kan_rpl_compiler_context_use_module (compiler_context, &intermediate);
    kan_rpl_compiler_context_set_option_flag (compiler_context, KAN_RPL_OPTION_TARGET_SCOPE_ANY,
                                              kan_string_intern ("wireframe"), KAN_TRUE);

    kan_rpl_compiler_instance_t meta_instance = kan_rpl_compiler_context_resolve (compiler_context, 0u, NULL);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (meta_instance))

    struct kan_rpl_meta_t meta;
    kan_rpl_meta_init (&meta);
    KAN_TEST_ASSERT (kan_rpl_compiler_instance_emit_meta (meta_instance, &meta, KAN_RPL_META_EMISSION_FULL))
    kan_rpl_compiler_instance_destroy (meta_instance);

#define TEST_FLOATING_TOLERANCE 0.000001f

    KAN_TEST_CHECK (meta.pipeline_type == KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC)
    KAN_TEST_CHECK (meta.graphics_classic_settings.polygon_mode == KAN_RPL_POLYGON_MODE_WIREFRAME)
    KAN_TEST_CHECK (meta.graphics_classic_settings.cull_mode == KAN_RPL_CULL_MODE_BACK)
    KAN_TEST_CHECK (meta.graphics_classic_settings.depth_test == KAN_TRUE)
    KAN_TEST_CHECK (meta.graphics_classic_settings.depth_write == KAN_TRUE)
    KAN_TEST_CHECK (meta.graphics_classic_settings.depth_bounds_test == KAN_FALSE)
    KAN_TEST_CHECK (meta.graphics_classic_settings.depth_compare_operation == KAN_RPL_COMPARE_OPERATION_LESS)
    KAN_TEST_CHECK (!meta.graphics_classic_settings.stencil_test)
    KAN_TEST_CHECK (fabs (meta.graphics_classic_settings.depth_min) < TEST_FLOATING_TOLERANCE)
    KAN_TEST_CHECK (fabs (meta.graphics_classic_settings.depth_max - 1.0) < TEST_FLOATING_TOLERANCE)

    KAN_TEST_ASSERT (meta.attribute_sources.size == 2u)
    struct kan_rpl_meta_attribute_source_t *attribute_source_meta =
        &((struct kan_rpl_meta_attribute_source_t *) meta.attribute_sources.data)[0u];
    KAN_TEST_CHECK (strcmp (attribute_source_meta->name, "vertex") == 0)
    KAN_TEST_CHECK (attribute_source_meta->binding == 0u)
    KAN_TEST_CHECK (attribute_source_meta->rate == KAN_RPL_META_ATTRIBUTE_SOURCE_RATE_VERTEX)
    KAN_TEST_CHECK (attribute_source_meta->block_size == 28u)

    KAN_TEST_ASSERT (attribute_source_meta->attributes.size == 5u)
    struct kan_rpl_meta_attribute_t *attribute_meta =
        &((struct kan_rpl_meta_attribute_t *) attribute_source_meta->attributes.data)[0u];
    KAN_TEST_CHECK (strcmp (attribute_meta->name, "position") == 0)
    KAN_TEST_CHECK (attribute_meta->location == 0u)
    KAN_TEST_CHECK (attribute_meta->offset == 0u)
    KAN_TEST_CHECK (attribute_meta->class == KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_3)
    KAN_TEST_CHECK (attribute_meta->item_format == KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_32)
    KAN_TEST_CHECK (attribute_meta->meta.size == 0u)

    attribute_meta = &((struct kan_rpl_meta_attribute_t *) attribute_source_meta->attributes.data)[1u];
    KAN_TEST_CHECK (strcmp (attribute_meta->name, "normal") == 0)
    KAN_TEST_CHECK (attribute_meta->location == 1u)
    KAN_TEST_CHECK (attribute_meta->offset == 12u)
    KAN_TEST_CHECK (attribute_meta->class == KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_3)
    KAN_TEST_CHECK (attribute_meta->item_format == KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_16)
    KAN_TEST_CHECK (attribute_meta->meta.size == 0u)

    attribute_meta = &((struct kan_rpl_meta_attribute_t *) attribute_source_meta->attributes.data)[2u];
    KAN_TEST_CHECK (strcmp (attribute_meta->name, "uv") == 0)
    KAN_TEST_CHECK (attribute_meta->location == 2u)
    KAN_TEST_CHECK (attribute_meta->offset == 18u)
    KAN_TEST_CHECK (attribute_meta->class == KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_2)
    KAN_TEST_CHECK (attribute_meta->item_format == KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_16)
    KAN_TEST_CHECK (attribute_meta->meta.size == 0u)

    attribute_meta = &((struct kan_rpl_meta_attribute_t *) attribute_source_meta->attributes.data)[3u];
    KAN_TEST_CHECK (strcmp (attribute_meta->name, "joint_indices") == 0)
    KAN_TEST_CHECK (attribute_meta->location == 3u)
    KAN_TEST_CHECK (attribute_meta->offset == 22u)
    KAN_TEST_CHECK (attribute_meta->class == KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_2)
    KAN_TEST_CHECK (attribute_meta->item_format == KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_16)
    KAN_TEST_CHECK (attribute_meta->meta.size == 0u)

    attribute_meta = &((struct kan_rpl_meta_attribute_t *) attribute_source_meta->attributes.data)[4u];
    KAN_TEST_CHECK (strcmp (attribute_meta->name, "joint_weights") == 0)
    KAN_TEST_CHECK (attribute_meta->location == 4u)
    KAN_TEST_CHECK (attribute_meta->offset == 26u)
    KAN_TEST_CHECK (attribute_meta->class == KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_2)
    KAN_TEST_CHECK (attribute_meta->item_format == KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_8)
    KAN_TEST_CHECK (attribute_meta->meta.size == 0u)

    attribute_source_meta = &((struct kan_rpl_meta_attribute_source_t *) meta.attribute_sources.data)[1u];
    KAN_TEST_CHECK (strcmp (attribute_source_meta->name, "instance_vertex") == 0)
    KAN_TEST_CHECK (attribute_source_meta->binding == 1u)
    KAN_TEST_CHECK (attribute_source_meta->rate == KAN_RPL_META_ATTRIBUTE_SOURCE_RATE_INSTANCE)
    KAN_TEST_CHECK (attribute_source_meta->block_size == 72u)

    KAN_TEST_ASSERT (attribute_source_meta->attributes.size == 3u)
    attribute_meta = &((struct kan_rpl_meta_attribute_t *) attribute_source_meta->attributes.data)[0u];
    KAN_TEST_CHECK (strcmp (attribute_meta->name, "color_multiplier") == 0)
    KAN_TEST_CHECK (attribute_meta->location == 5u)
    KAN_TEST_CHECK (attribute_meta->offset == 0u)
    KAN_TEST_CHECK (attribute_meta->class == KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_4)
    KAN_TEST_CHECK (attribute_meta->item_format == KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_8)
    KAN_TEST_CHECK (attribute_meta->meta.size == 0u)

    attribute_meta = &((struct kan_rpl_meta_attribute_t *) attribute_source_meta->attributes.data)[1u];
    KAN_TEST_CHECK (strcmp (attribute_meta->name, "joint_offset") == 0)
    KAN_TEST_CHECK (attribute_meta->location == 6u)
    KAN_TEST_CHECK (attribute_meta->offset == 4u)
    KAN_TEST_CHECK (attribute_meta->class == KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_1)
    KAN_TEST_CHECK (attribute_meta->item_format == KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_32)
    KAN_TEST_ASSERT (attribute_meta->meta.size == 2u)
    KAN_TEST_CHECK (strcmp (((kan_interned_string_t *) attribute_meta->meta.data)[0u], "joint_offset_index") == 0)
    KAN_TEST_CHECK (strcmp (((kan_interned_string_t *) attribute_meta->meta.data)[1u], "hidden") == 0)

    attribute_meta = &((struct kan_rpl_meta_attribute_t *) attribute_source_meta->attributes.data)[2u];
    KAN_TEST_CHECK (strcmp (attribute_meta->name, "model_space") == 0)
    KAN_TEST_CHECK (attribute_meta->location == 7u)
    KAN_TEST_CHECK (attribute_meta->offset == 8u)
    KAN_TEST_CHECK (attribute_meta->class == KAN_RPL_META_ATTRIBUTE_CLASS_MATRIX_4X4)
    KAN_TEST_CHECK (attribute_meta->item_format == KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_32)
    KAN_TEST_ASSERT (attribute_meta->meta.size == 2u)
    KAN_TEST_CHECK (strcmp (((kan_interned_string_t *) attribute_meta->meta.data)[0u], "model_space_matrix") == 0)
    KAN_TEST_CHECK (strcmp (((kan_interned_string_t *) attribute_meta->meta.data)[1u], "hidden") == 0)

    KAN_TEST_CHECK (meta.push_constant_size == 0u)

    KAN_TEST_ASSERT (meta.set_pass.buffers.size == 1u)
    struct kan_rpl_meta_buffer_t *buffer_meta = &((struct kan_rpl_meta_buffer_t *) meta.set_pass.buffers.data)[0u];
    KAN_TEST_CHECK (strcmp (buffer_meta->name, "pass") == 0)
    KAN_TEST_CHECK (buffer_meta->binding == 0u)
    KAN_TEST_CHECK (buffer_meta->type == KAN_RPL_BUFFER_TYPE_UNIFORM)
    KAN_TEST_CHECK (buffer_meta->main_size == 64u)

    KAN_TEST_ASSERT (buffer_meta->main_parameters.size == 1u)
    struct kan_rpl_meta_parameter_t *parameter_meta =
        &((struct kan_rpl_meta_parameter_t *) buffer_meta->main_parameters.data)[0u];
    KAN_TEST_CHECK (strcmp (parameter_meta->name, "projection_mul_view") == 0)
    KAN_TEST_CHECK (parameter_meta->type == KAN_RPL_META_VARIABLE_TYPE_F4X4)
    KAN_TEST_CHECK (parameter_meta->offset == 0u)
    KAN_TEST_CHECK (parameter_meta->total_item_count == 1u)
    KAN_TEST_ASSERT (parameter_meta->meta.size == 2u)
    KAN_TEST_CHECK (strcmp (((kan_interned_string_t *) parameter_meta->meta.data)[0u], "projection_view_matrix") == 0)
    KAN_TEST_CHECK (strcmp (((kan_interned_string_t *) parameter_meta->meta.data)[1u], "hidden") == 0)

    KAN_TEST_ASSERT (buffer_meta->tail_item_parameters.size == 0u)

    KAN_TEST_ASSERT (meta.set_material.buffers.size == 0u)
    KAN_TEST_ASSERT (meta.set_object.buffers.size == 0u)
    KAN_TEST_ASSERT (meta.set_shared.buffers.size == 1u)

    buffer_meta = &((struct kan_rpl_meta_buffer_t *) meta.set_shared.buffers.data)[0u];
    KAN_TEST_CHECK (strcmp (buffer_meta->name, "joints") == 0)
    KAN_TEST_CHECK (buffer_meta->binding == 0u)
    KAN_TEST_CHECK (buffer_meta->type == KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE)
    KAN_TEST_CHECK (buffer_meta->main_size == 0u)
    KAN_TEST_CHECK (buffer_meta->tail_item_size == 64u)

    KAN_TEST_ASSERT (buffer_meta->main_parameters.size == 0u)

    KAN_TEST_ASSERT (buffer_meta->tail_item_parameters.size == 0u)

    KAN_TEST_ASSERT (meta.set_pass.samplers.size == 0u)
    KAN_TEST_ASSERT (meta.set_material.samplers.size == 1u)
    KAN_TEST_ASSERT (meta.set_object.samplers.size == 0u)
    KAN_TEST_ASSERT (meta.set_shared.samplers.size == 0u)

    struct kan_rpl_meta_sampler_t *sampler_meta =
        &((struct kan_rpl_meta_sampler_t *) meta.set_material.samplers.data)[0u];
    KAN_TEST_CHECK (sampler_meta->binding == 0u)

    KAN_TEST_ASSERT (meta.set_pass.images.size == 0u)
    KAN_TEST_ASSERT (meta.set_material.images.size == 1u)
    KAN_TEST_ASSERT (meta.set_object.images.size == 0u)
    KAN_TEST_ASSERT (meta.set_shared.images.size == 0u)

    struct kan_rpl_meta_image_t *image_meta = &((struct kan_rpl_meta_image_t *) meta.set_material.images.data)[0u];
    KAN_TEST_CHECK (image_meta->binding == 1u)
    KAN_TEST_CHECK (image_meta->type == KAN_RPL_IMAGE_TYPE_COLOR_2D)
    KAN_TEST_CHECK (image_meta->image_array_size == 4u)

    KAN_TEST_ASSERT (meta.color_outputs.size == 1u)
    struct kan_rpl_meta_color_output_t *color_output =
        &((struct kan_rpl_meta_color_output_t *) meta.color_outputs.data)[0u];
    KAN_TEST_CHECK (color_output->components_count == 4u)
    KAN_TEST_CHECK (color_output->use_blend)
    KAN_TEST_CHECK (color_output->write_r)
    KAN_TEST_CHECK (color_output->write_g)
    KAN_TEST_CHECK (color_output->write_b)
    KAN_TEST_CHECK (color_output->write_a)
    KAN_TEST_CHECK (color_output->source_color_blend_factor == KAN_RPL_BLEND_FACTOR_ONE)
    KAN_TEST_CHECK (color_output->destination_color_blend_factor == KAN_RPL_BLEND_FACTOR_ZERO)
    KAN_TEST_CHECK (color_output->color_blend_operation == KAN_RPL_BLEND_OPERATION_ADD)
    KAN_TEST_CHECK (color_output->source_alpha_blend_factor == KAN_RPL_BLEND_FACTOR_ONE)
    KAN_TEST_CHECK (color_output->destination_alpha_blend_factor == KAN_RPL_BLEND_FACTOR_ZERO)
    KAN_TEST_CHECK (color_output->alpha_blend_operation == KAN_RPL_BLEND_OPERATION_ADD)

    KAN_TEST_CHECK (fabs (meta.color_blend_constants.r) < TEST_FLOATING_TOLERANCE)
    KAN_TEST_CHECK (fabs (meta.color_blend_constants.g) < TEST_FLOATING_TOLERANCE)
    KAN_TEST_CHECK (fabs (meta.color_blend_constants.b) < TEST_FLOATING_TOLERANCE)
    KAN_TEST_CHECK (fabs (meta.color_blend_constants.a) < TEST_FLOATING_TOLERANCE)

#undef TEST_FLOATING_TOLERANCE

    kan_rpl_meta_shutdown (&meta);
    struct kan_dynamic_array_t code;
    compile_pipeline (compiler_context, &code);
    save_code ("result.spirv", &code);

    // Additional optional check: if we have spirv-val, lets run it.
    if (system ("spirv-val --help") == 0)
    {
        KAN_TEST_CHECK (system ("spirv-val --target-env vulkan1.1 result.spirv") == 0)
    }

    kan_dynamic_array_shutdown (&code);
    kan_rpl_compiler_context_destroy (compiler_context);
    kan_rpl_intermediate_shutdown (&intermediate);
}

static void compile_test (const char *path)
{
    struct kan_dynamic_array_t source;
    load_pipeline_source (path, &source);

    kan_rpl_parser_t parser = kan_rpl_parser_create (kan_string_intern ("test"));
    KAN_TEST_ASSERT (kan_rpl_parser_add_source (parser, (const char *) source.data, kan_string_intern ("code")))

    struct kan_rpl_intermediate_t intermediate;
    kan_rpl_intermediate_init (&intermediate);
    KAN_TEST_ASSERT (kan_rpl_parser_build_intermediate (parser, &intermediate))

    kan_rpl_parser_destroy (parser);
    kan_dynamic_array_shutdown (&source);

    kan_rpl_compiler_context_t compiler_context =
        kan_rpl_compiler_context_create (KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC, kan_string_intern ("variant_test"));
    kan_rpl_compiler_context_use_module (compiler_context, &intermediate);

    struct kan_dynamic_array_t code;
    compile_pipeline (compiler_context, &code);
    save_code ("result.spirv", &code);

    // Additional optional check: if we have spirv-val, lets run it.
    if (system ("spirv-val --help") == 0)
    {
        KAN_TEST_CHECK (system ("spirv-val --target-env vulkan1.1 result.spirv") == 0)
    }

    kan_dynamic_array_shutdown (&code);
    kan_rpl_compiler_context_destroy (compiler_context);
    kan_rpl_intermediate_shutdown (&intermediate);
}

KAN_TEST_CASE (basic_compile)
{
    compile_test (PIPELINE_BASE_PATH "basic.rpl");
}

KAN_TEST_CASE (if_compile)
{
    compile_test (PIPELINE_BASE_PATH "if.rpl");
}

KAN_TEST_CASE (while_compile)
{
    compile_test (PIPELINE_BASE_PATH "while.rpl");
}

KAN_TEST_CASE (for_compile)
{
    compile_test (PIPELINE_BASE_PATH "for.rpl");
}

static void benchmark_step (struct kan_dynamic_array_t *source, kan_bool_t finishing_iteration)
{
    kan_rpl_parser_t parser = kan_rpl_parser_create (kan_string_intern ("test"));
    KAN_TEST_CHECK (kan_rpl_parser_add_source (parser, (const char *) source->data, kan_string_intern ("code")))

    struct kan_rpl_intermediate_t intermediate;
    kan_rpl_intermediate_init (&intermediate);
    KAN_TEST_CHECK (kan_rpl_parser_build_intermediate (parser, &intermediate))
    kan_rpl_parser_destroy (parser);

    kan_rpl_compiler_context_t compiler_context =
        kan_rpl_compiler_context_create (KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC, kan_string_intern ("variant_test"));
    kan_rpl_compiler_context_use_module (compiler_context, &intermediate);

    kan_rpl_compiler_instance_t meta_instance = kan_rpl_compiler_context_resolve (compiler_context, 0u, NULL);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (meta_instance))

    struct kan_rpl_meta_t meta;
    kan_rpl_meta_init (&meta);

    KAN_TEST_ASSERT (kan_rpl_compiler_instance_emit_meta (meta_instance, &meta, KAN_RPL_META_EMISSION_FULL))
    kan_rpl_compiler_instance_destroy (meta_instance);
    kan_rpl_meta_shutdown (&meta);

    struct kan_dynamic_array_t code;
    compile_pipeline (compiler_context, &code);

    if (finishing_iteration)
    {
        save_code ("result.spirv", &code);
        printf ("Generated code words count: %lu.", (unsigned long) code.size);
    }

    kan_dynamic_array_shutdown (&code);
    kan_rpl_compiler_context_destroy (compiler_context);
    kan_rpl_intermediate_shutdown (&intermediate);
}

KAN_TEST_CASE (benchmark)
{
    struct kan_dynamic_array_t source;
    load_pipeline_source (PIPELINE_BASE_PATH "benchmark.rpl", &source);

#define BENCHMARK_CYCLES 2500u
    clock_t benchmark_begin = clock ();

    for (kan_loop_size_t index = 0u; index < BENCHMARK_CYCLES; ++index)
    {
        benchmark_step (&source, KAN_FALSE);
    }

    clock_t benchmark_end = clock ();
    float total_ms = (float) (benchmark_end - benchmark_begin) * 1000.0f / (float) CLOCKS_PER_SEC;
    printf ("Total: %f ms.\nAverage per run: %f ms.\n", total_ms, total_ms / (float) BENCHMARK_CYCLES);

    // Last cycle to save results so it can be inspected.
    benchmark_step (&source, KAN_TRUE);
    kan_dynamic_array_shutdown (&source);
}
