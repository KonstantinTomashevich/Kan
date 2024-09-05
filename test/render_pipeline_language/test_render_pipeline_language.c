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

#define PIPELINE_BASE_PATH "../../pipelines/"

static void load_pipeline_source (const char *path, struct kan_dynamic_array_t *output)
{
    struct kan_stream_t *file_stream = kan_direct_file_stream_open_for_read (path, KAN_TRUE);
    KAN_TEST_ASSERT (file_stream)

    KAN_TEST_ASSERT (file_stream->operations->seek (file_stream, KAN_STREAM_SEEK_END, 0))
    uint64_t size = file_stream->operations->tell (file_stream);
    KAN_TEST_ASSERT (file_stream->operations->seek (file_stream, KAN_STREAM_SEEK_START, 0))

    kan_dynamic_array_init (output, size + 1u, sizeof (char), _Alignof (char), KAN_ALLOCATION_GROUP_IGNORE);
    output->size = size + 1u;
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
    KAN_TEST_ASSERT (code_instance != KAN_INVALID_RPL_COMPILER_INSTANCE)

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
    kan_rpl_compiler_context_set_option_count (compiler_context, kan_string_intern ("max_joints"), 1024u);
    kan_rpl_compiler_context_set_option_flag (compiler_context, kan_string_intern ("wireframe"), KAN_TRUE);

    kan_rpl_compiler_instance_t meta_instance = kan_rpl_compiler_context_resolve (compiler_context, 0u, NULL);
    KAN_TEST_ASSERT (meta_instance != KAN_INVALID_RPL_COMPILER_INSTANCE)

    struct kan_rpl_meta_t meta;
    kan_rpl_meta_init (&meta);
    KAN_TEST_ASSERT (kan_rpl_compiler_instance_emit_meta (meta_instance, &meta))
    kan_rpl_compiler_instance_destroy (meta_instance);

    KAN_TEST_CHECK (meta.pipeline_type == KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC)
    KAN_TEST_CHECK (meta.graphics_classic_settings.polygon_mode == KAN_RPL_POLYGON_MODE_WIREFRAME)
    KAN_TEST_CHECK (meta.graphics_classic_settings.cull_mode == KAN_RPL_CULL_MODE_BACK)
    KAN_TEST_CHECK (meta.graphics_classic_settings.depth_test == KAN_TRUE)
    KAN_TEST_CHECK (meta.graphics_classic_settings.depth_write == KAN_TRUE)

    KAN_TEST_ASSERT (meta.buffers.size == 3u)
    struct kan_rpl_meta_buffer_t *buffer_meta = &((struct kan_rpl_meta_buffer_t *) meta.buffers.data)[0u];
    KAN_TEST_CHECK (strcmp (buffer_meta->name, "vertex") == 0)
    KAN_TEST_CHECK (buffer_meta->set == 0u)
    KAN_TEST_CHECK (buffer_meta->binding == 0u)
    KAN_TEST_CHECK (buffer_meta->type == KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE)
    KAN_TEST_CHECK (buffer_meta->size == 48u)

    KAN_TEST_ASSERT (buffer_meta->attributes.size == 5u)
    struct kan_rpl_meta_attribute_t *attribute_meta =
        &((struct kan_rpl_meta_attribute_t *) buffer_meta->attributes.data)[0u];
    KAN_TEST_CHECK (attribute_meta->location == 0u)
    KAN_TEST_CHECK (attribute_meta->type == KAN_RPL_META_VARIABLE_TYPE_F3)
    KAN_TEST_CHECK (attribute_meta->offset == 0u)

    attribute_meta = &((struct kan_rpl_meta_attribute_t *) buffer_meta->attributes.data)[1u];
    KAN_TEST_CHECK (attribute_meta->location == 1u)
    KAN_TEST_CHECK (attribute_meta->type == KAN_RPL_META_VARIABLE_TYPE_F3)
    KAN_TEST_CHECK (attribute_meta->offset == 12u)

    attribute_meta = &((struct kan_rpl_meta_attribute_t *) buffer_meta->attributes.data)[2u];
    KAN_TEST_CHECK (attribute_meta->location == 2u)
    KAN_TEST_CHECK (attribute_meta->type == KAN_RPL_META_VARIABLE_TYPE_F2)
    KAN_TEST_CHECK (attribute_meta->offset == 24u)

    attribute_meta = &((struct kan_rpl_meta_attribute_t *) buffer_meta->attributes.data)[3u];
    KAN_TEST_CHECK (attribute_meta->location == 3u)
    KAN_TEST_CHECK (attribute_meta->type == KAN_RPL_META_VARIABLE_TYPE_I2)
    KAN_TEST_CHECK (attribute_meta->offset == 32u)

    attribute_meta = &((struct kan_rpl_meta_attribute_t *) buffer_meta->attributes.data)[4u];
    KAN_TEST_CHECK (attribute_meta->location == 4u)
    KAN_TEST_CHECK (attribute_meta->type == KAN_RPL_META_VARIABLE_TYPE_F2)
    KAN_TEST_CHECK (attribute_meta->offset == 40u)

    KAN_TEST_ASSERT (buffer_meta->parameters.size == 0u)

    buffer_meta = &((struct kan_rpl_meta_buffer_t *) meta.buffers.data)[1u];
    KAN_TEST_CHECK (strcmp (buffer_meta->name, "instance_storage") == 0)
    KAN_TEST_CHECK (buffer_meta->set == 1u)
    KAN_TEST_CHECK (buffer_meta->binding == 0u)
    KAN_TEST_CHECK (!buffer_meta->stable_binding)
    KAN_TEST_CHECK (buffer_meta->type == KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE)
    KAN_TEST_CHECK (buffer_meta->size == 65552u)

    KAN_TEST_ASSERT (buffer_meta->attributes.size == 0u)

    KAN_TEST_ASSERT (buffer_meta->parameters.size == 2u)
    struct kan_rpl_meta_parameter_t *parameter_meta =
        &((struct kan_rpl_meta_parameter_t *) buffer_meta->parameters.data)[0u];
    KAN_TEST_CHECK (strcmp (parameter_meta->name, "color_multiplier") == 0)
    KAN_TEST_CHECK (parameter_meta->type == KAN_RPL_META_VARIABLE_TYPE_F4)
    KAN_TEST_CHECK (parameter_meta->offset == 0u)
    KAN_TEST_CHECK (parameter_meta->total_item_count == 1u)
    KAN_TEST_ASSERT (parameter_meta->meta.size == 0u)

    parameter_meta = &((struct kan_rpl_meta_parameter_t *) buffer_meta->parameters.data)[1u];
    KAN_TEST_CHECK (strcmp (parameter_meta->name, "joint_data.model_joints") == 0)
    KAN_TEST_CHECK (parameter_meta->type == KAN_RPL_META_VARIABLE_TYPE_F4X4)
    KAN_TEST_CHECK (parameter_meta->offset == 16u)
    KAN_TEST_CHECK (parameter_meta->total_item_count == 1024u)
    KAN_TEST_ASSERT (parameter_meta->meta.size == 2u)
    KAN_TEST_CHECK (strcmp (((kan_interned_string_t *) parameter_meta->meta.data)[0u], "model_joint_matrices") == 0)
    KAN_TEST_CHECK (strcmp (((kan_interned_string_t *) parameter_meta->meta.data)[1u], "hidden") == 0)

    buffer_meta = &((struct kan_rpl_meta_buffer_t *) meta.buffers.data)[2u];
    KAN_TEST_CHECK (strcmp (buffer_meta->name, "uniforms") == 0)
    KAN_TEST_CHECK (buffer_meta->set == 0u)
    KAN_TEST_CHECK (buffer_meta->binding == 0u)
    KAN_TEST_CHECK (buffer_meta->stable_binding)
    KAN_TEST_CHECK (buffer_meta->type == KAN_RPL_BUFFER_TYPE_UNIFORM)
    KAN_TEST_CHECK (buffer_meta->size == 64u)

    KAN_TEST_ASSERT (buffer_meta->attributes.size == 0u)

    KAN_TEST_ASSERT (buffer_meta->parameters.size == 1u)
    parameter_meta = &((struct kan_rpl_meta_parameter_t *) buffer_meta->parameters.data)[0u];
    KAN_TEST_CHECK (strcmp (parameter_meta->name, "projection_mul_view") == 0)
    KAN_TEST_CHECK (parameter_meta->type == KAN_RPL_META_VARIABLE_TYPE_F4X4)
    KAN_TEST_CHECK (parameter_meta->offset == 0u)
    KAN_TEST_CHECK (parameter_meta->total_item_count == 1u)
    KAN_TEST_ASSERT (parameter_meta->meta.size == 2u)
    KAN_TEST_CHECK (strcmp (((kan_interned_string_t *) parameter_meta->meta.data)[0u], "projection_view_matrix") == 0)
    KAN_TEST_CHECK (strcmp (((kan_interned_string_t *) parameter_meta->meta.data)[1u], "hidden") == 0)

    KAN_TEST_ASSERT (meta.samplers.size == 1u)
    struct kan_rpl_meta_sampler_t *sampler_meta = &((struct kan_rpl_meta_sampler_t *) meta.samplers.data)[0u];
    KAN_TEST_CHECK (sampler_meta->set == 0u)
    KAN_TEST_CHECK (sampler_meta->binding == 1u)
    KAN_TEST_CHECK (sampler_meta->type == KAN_RPL_SAMPLER_TYPE_2D)
    KAN_TEST_CHECK (sampler_meta->settings.mag_filter == KAN_RPL_META_SAMPLER_FILTER_NEAREST)
    KAN_TEST_CHECK (sampler_meta->settings.min_filter == KAN_RPL_META_SAMPLER_FILTER_NEAREST)
    KAN_TEST_CHECK (sampler_meta->settings.mip_map_mode == KAN_RPL_META_SAMPLER_MIP_MAP_MODE_NEAREST)
    KAN_TEST_CHECK (sampler_meta->settings.address_mode_u == KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT)
    KAN_TEST_CHECK (sampler_meta->settings.address_mode_v == KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT)
    KAN_TEST_CHECK (sampler_meta->settings.address_mode_w == KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT)

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
    KAN_TEST_CHECK (kan_rpl_parser_add_source (parser, (const char *) source.data, kan_string_intern ("code")))

    struct kan_rpl_intermediate_t intermediate;
    kan_rpl_intermediate_init (&intermediate);
    KAN_TEST_CHECK (kan_rpl_parser_build_intermediate (parser, &intermediate))

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
    KAN_TEST_ASSERT (meta_instance != KAN_INVALID_RPL_COMPILER_INSTANCE)

    struct kan_rpl_meta_t meta;
    kan_rpl_meta_init (&meta);

    KAN_TEST_ASSERT (kan_rpl_compiler_instance_emit_meta (meta_instance, &meta))
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

#define BENCHMARK_CYCLES 10000u
    clock_t benchmark_begin = clock ();

    for (uint64_t index = 0u; index < BENCHMARK_CYCLES; ++index)
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
