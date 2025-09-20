#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <resource_test_expectation_build_api.h>

#include <string.h>

#include <kan/file_system/stream.h>
#include <kan/image/image.h>
#include <kan/log/logging.h>
#include <kan/resource_pipeline/meta.h>
#include <kan/stream/random_access_stream_buffer.h>
#include <kan/test_expectation/test_expectation.h>

KAN_LOG_DEFINE_CATEGORY (test_expectation_build);

static enum kan_resource_build_rule_result_t test_expectation_build (struct kan_resource_build_rule_context_t *context);

KAN_REFLECTION_STRUCT_META (test_expectation_t)
RESOURCE_TEST_EXPECTATION_BUILD_API struct kan_resource_build_rule_t test_expectation_build_rule = {
    .primary_input_type = NULL,
    .platform_configuration_type = NULL,
    .secondary_types_count = 0u,
    .secondary_types = NULL,
    .functor = test_expectation_build,
    .version = CUSHION_START_NS_X64,
};

enum kan_resource_build_rule_result_t test_expectation_build (struct kan_resource_build_rule_context_t *context)
{
    struct test_expectation_t *output = context->primary_output;
    struct kan_stream_t *load_stream = kan_direct_file_stream_open_for_read (context->primary_third_party_path, true);

    if (!load_stream)
    {
        KAN_LOG (test_expectation_build, KAN_LOG_ERROR, "Failed to open image at \"%s\" for test expectation \"%s\".",
                 context->primary_third_party_path, context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    load_stream = kan_random_access_stream_buffer_open_for_read (load_stream, 16384u);
    CUSHION_DEFER { load_stream->operations->close (load_stream); }

    struct kan_image_raw_data_t image_data;
    kan_image_raw_data_init (&image_data);
    CUSHION_DEFER { kan_image_raw_data_shutdown (&image_data); }

    if (!kan_image_load (load_stream, &image_data))
    {
        KAN_LOG (test_expectation_build, KAN_LOG_ERROR, "Failed to load image at \"%s\" for texture header \"%s\".",
                 context->primary_third_party_path, context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    output->width = image_data.width;
    output->height = image_data.height;

    kan_dynamic_array_set_capacity (&output->rgba_data, output->width * output->height);
    output->rgba_data.size = output->rgba_data.capacity;
    memcpy (output->rgba_data.data, image_data.data, output->rgba_data.size * sizeof (uint32_t));
    return KAN_RESOURCE_BUILD_RULE_SUCCESS;
}
