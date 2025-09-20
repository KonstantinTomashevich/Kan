#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <string.h>

#include <kan/api_common/min_max.h>
#include <kan/file_system/stream.h>
#include <kan/image/image.h>
#include <kan/inline_math/inline_math.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/render_foundation/resource_texture_build.h>
#include <kan/resource_pipeline/meta.h>
#include <kan/stream/random_access_stream_buffer.h>

KAN_LOG_DEFINE_CATEGORY (resource_render_foundation_texture);
KAN_USE_STATIC_INTERNED_IDS

void kan_resource_texture_platform_configuration_init (struct kan_resource_texture_platform_configuration_t *instance)
{
    kan_dynamic_array_init (&instance->supported_formats, 0u, sizeof (enum kan_resource_texture_format_t),
                            alignof (enum kan_resource_texture_format_t), kan_allocation_group_stack_get ());
}

void kan_resource_texture_platform_configuration_shutdown (
    struct kan_resource_texture_platform_configuration_t *instance)
{
    kan_dynamic_array_shutdown (&instance->supported_formats);
}

KAN_REFLECTION_STRUCT_META (kan_resource_texture_build_preset_t)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_type_meta_t kan_resource_texture_build_preset_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

void kan_resource_texture_build_preset_init (struct kan_resource_texture_build_preset_t *instance)
{
    instance->mip_generation = KAN_RESOURCE_TEXTURE_MIP_GENERATION_AVERAGE;
    instance->target_mips = 1u;
    kan_dynamic_array_init (&instance->supported_target_formats, 0u, sizeof (enum kan_resource_texture_format_t),
                            alignof (enum kan_resource_texture_format_t), kan_allocation_group_stack_get ());
}

void kan_resource_texture_build_preset_shutdown (struct kan_resource_texture_build_preset_t *instance)
{
    kan_dynamic_array_shutdown (&instance->supported_target_formats);
}

KAN_REFLECTION_STRUCT_META (kan_resource_texture_header_t)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_type_meta_t kan_resource_texture_header_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_texture_header_t, preset)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_reference_meta_t kan_resource_texture_header_reference_preset =
    {
        .type_name = "kan_resource_texture_build_preset_t",
        .flags = 0u,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_texture_header_t, image)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_reference_meta_t kan_resource_texture_header_reference_image =
    {
        .type_name = NULL,
        .flags = 0u,
};

void kan_resource_texture_header_init (struct kan_resource_texture_header_t *instance)
{
    instance->preset = NULL;
    instance->image_class = KAN_RESOURCE_TEXTURE_IMAGE_CLASS_COLOR_SRGB;
    instance->image = NULL;
}

static enum kan_resource_build_rule_result_t texture_build (struct kan_resource_build_rule_context_t *context);

KAN_REFLECTION_STRUCT_META (kan_resource_texture_t)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_build_rule_t kan_resource_texture_build_rule = {
    .primary_input_type = "kan_resource_texture_header_t",
    .platform_configuration_type = "kan_resource_texture_platform_configuration_t",
    .secondary_types_count = 1u,
    .secondary_types = (const char *[]) {"kan_resource_texture_build_preset_t"},
    .functor = texture_build,
    .version = CUSHION_START_NS_X64,
};

static void free_transitive_mip_data (float **image_mips,
                                      kan_instance_size_t channels,
                                      struct kan_resource_texture_t *output,
                                      kan_allocation_group_t mips_allocation_group)
{
    for (kan_loop_size_t mip = 0u; mip < (kan_loop_size_t) output->mips; ++mip)
    {
        const kan_instance_size_t width = output->width >> mip;
        const kan_instance_size_t height = output->height >> mip;
        kan_free_general (mips_allocation_group, image_mips[mip], width * height * channels * sizeof (float));
    }

    kan_free_general (mips_allocation_group, image_mips, sizeof (float *) * output->mips);
}

static enum kan_resource_build_rule_result_t texture_build (struct kan_resource_build_rule_context_t *context)
{
    const struct kan_resource_texture_header_t *input = context->primary_input;
    struct kan_resource_texture_t *output = context->primary_output;
    const struct kan_resource_texture_platform_configuration_t *configuration = context->platform_configuration;

    if (!input->preset)
    {
        KAN_LOG (resource_render_foundation_texture, KAN_LOG_ERROR,
                 "Texture header \"%s\" has no specified preset and therefore cannot be built.", context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    if (!input->image)
    {
        KAN_LOG (resource_render_foundation_texture, KAN_LOG_ERROR,
                 "Texture header \"%s\" has no specified image and therefore cannot be built.", context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    kan_static_interned_ids_ensure_initialized ();
    const struct kan_resource_texture_build_preset_t *preset = NULL;
    const char *image_path = NULL;

    struct kan_resource_build_rule_secondary_node_t *secondary_node = context->secondary_input_first;
    while (secondary_node)
    {
        if (secondary_node->type == KAN_STATIC_INTERNED_ID_GET (kan_resource_texture_build_preset_t) &&
            secondary_node->name == input->preset)
        {
            preset = secondary_node->data;
        }
        else if (!secondary_node->type && secondary_node->name == input->image)
        {
            image_path = secondary_node->third_party_path;
        }

        secondary_node = secondary_node->next;
    }

    // Should never happen.
    KAN_ASSERT (preset)
    KAN_ASSERT (image_path)

    // We use floating point numbers as we would still need to use them in lots of cases, and it is better to avoid
    // compressing transitive data back-and-forth from float and to float. For example, to properly generate averaged
    // mips for srgb-encoded data we need to transfer it to rgb first and then to srgb back if target format is srgb,
    // which is usually the case.
    float **image_mips = NULL;
    kan_instance_size_t image_channels = 0u;

    kan_allocation_group_t main_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "resource_render_foundation_texture_build");
    kan_allocation_group_t mips_allocation_group = kan_allocation_group_get_child (main_allocation_group, "mips");

    // Scope for defers.
    {
        struct kan_stream_t *image_load_stream = kan_direct_file_stream_open_for_read (image_path, true);
        if (!image_load_stream)
        {
            KAN_LOG (resource_render_foundation_texture, KAN_LOG_ERROR,
                     "Failed to open image at \"%s\" for texture header \"%s\".", context->primary_third_party_path,
                     context->primary_name)
            return KAN_RESOURCE_BUILD_RULE_FAILURE;
        }

        image_load_stream =
            kan_random_access_stream_buffer_open_for_read (image_load_stream, KAN_RESOURCE_RF_TEXTURE_LOAD_BUFFER);
        CUSHION_DEFER { image_load_stream->operations->close (image_load_stream); }

        struct kan_image_raw_data_t image_data;
        kan_image_raw_data_init (&image_data);
        CUSHION_DEFER { kan_image_raw_data_shutdown (&image_data); }

        if (!kan_image_load (image_load_stream, &image_data))
        {
            KAN_LOG (resource_render_foundation_texture, KAN_LOG_ERROR,
                     "Failed to load image at \"%s\" for texture header \"%s\".", context->primary_third_party_path,
                     context->primary_name)
            return KAN_RESOURCE_BUILD_RULE_FAILURE;
        }

        output->width = image_data.width;
        output->height = image_data.height;

        // Calculate best mip: closest to target, but not zero-sized.
        output->mips = 1u;
        kan_instance_size_t check_width = output->width;
        kan_instance_size_t check_height = output->height;

        while (output->mips < preset->target_mips)
        {
            check_width >>= 1u;
            check_height >>= 1u;

            if (check_width == 0u || check_height == 0u)
            {
                // Cannot use more mips.
                break;
            }

            ++output->mips;
        }

        // Calculate decoded channel count.
        switch (input->image_class)
        {
        case KAN_RESOURCE_TEXTURE_IMAGE_CLASS_COLOR_SRGB:
        case KAN_RESOURCE_TEXTURE_IMAGE_CLASS_COLOR_RGB:
            image_channels = 4u;
            break;

        case KAN_RESOURCE_TEXTURE_IMAGE_CLASS_DEPTH_GRAYSCALE:
        case KAN_RESOURCE_TEXTURE_IMAGE_CLASS_DEPTH_FLOAT_32:
            image_channels = 1u;
            break;
        }

        // Allocate data arrays per mip.
        image_mips = kan_allocate_general (mips_allocation_group, sizeof (float *) * output->mips, alignof (float *));

        for (kan_loop_size_t mip = 0u; mip < (kan_loop_size_t) output->mips; ++mip)
        {
            const kan_instance_size_t width = output->width >> mip;
            KAN_ASSERT (width > 0u)
            const kan_instance_size_t height = output->height >> mip;
            KAN_ASSERT (height > 0u)
            image_mips[mip] = kan_allocate_general (mips_allocation_group,
                                                    width * height * image_channels * sizeof (float), alignof (float));
        }

        // Properly decode first mip from image data.
        const kan_loop_size_t source_pixel_count = image_data.width * image_data.height;
        const uint8_t *source_pixel = image_data.data;
        float *target_pixel = image_mips[0u];

        switch (input->image_class)
        {
        case KAN_RESOURCE_TEXTURE_IMAGE_CLASS_COLOR_SRGB:
            for (kan_loop_size_t pixel_index = 0u; pixel_index < source_pixel_count;
                 ++pixel_index, source_pixel += sizeof (uint32_t), target_pixel += 4u)
            {
                target_pixel[0u] = kan_color_transfer_srgb_to_rgb ((float) source_pixel[0u] / 255.0f);
                target_pixel[1u] = kan_color_transfer_srgb_to_rgb ((float) source_pixel[1u] / 255.0f);
                target_pixel[2u] = kan_color_transfer_srgb_to_rgb ((float) source_pixel[2u] / 255.0f);
                target_pixel[3u] = kan_color_transfer_srgb_to_rgb ((float) source_pixel[3u] / 255.0f);
            }

            break;

        case KAN_RESOURCE_TEXTURE_IMAGE_CLASS_COLOR_RGB:
            for (kan_loop_size_t pixel_index = 0u; pixel_index < source_pixel_count;
                 ++pixel_index, source_pixel += sizeof (uint32_t), target_pixel += 4u)
            {
                target_pixel[0u] = (float) source_pixel[0u] / 255.0f;
                target_pixel[1u] = (float) source_pixel[1u] / 255.0f;
                target_pixel[2u] = (float) source_pixel[2u] / 255.0f;
                target_pixel[3u] = (float) source_pixel[3u] / 255.0f;
            }

            break;

        case KAN_RESOURCE_TEXTURE_IMAGE_CLASS_DEPTH_GRAYSCALE:
            for (kan_loop_size_t pixel_index = 0u; pixel_index < source_pixel_count;
                 ++pixel_index, source_pixel += sizeof (uint32_t), ++target_pixel)
            {
                *target_pixel = (float) source_pixel[0u] / 255.0f;
            }

            break;

        case KAN_RESOURCE_TEXTURE_IMAGE_CLASS_DEPTH_FLOAT_32:
            for (kan_loop_size_t pixel_index = 0u; pixel_index < source_pixel_count;
                 ++pixel_index, source_pixel += sizeof (uint32_t), ++target_pixel)
            {
                *target_pixel = *(float *) source_pixel;
            }

            break;
        }
    }

    CUSHION_DEFER { free_transitive_mip_data (image_mips, image_channels, output, mips_allocation_group); };
    for (kan_loop_size_t next_mip = 1u; next_mip < output->mips; ++next_mip)
    {
        const kan_instance_size_t source_width = output->width >> (next_mip - 1u);
        KAN_ASSERT (source_width > 0u)
        const kan_instance_size_t source_height = output->height >> (next_mip - 1u);
        KAN_ASSERT (source_height > 0u)

        const kan_instance_size_t mip_width = output->width >> next_mip;
        KAN_ASSERT (mip_width > 0u)
        const kan_instance_size_t mip_height = output->height >> next_mip;
        KAN_ASSERT (mip_height > 0u)

        const float *source_data = image_mips[next_mip - 1u];
        float *target_data = image_mips[next_mip];

        for (kan_loop_size_t x = 0u; x < (kan_loop_size_t) mip_width; ++x)
        {
            for (kan_loop_size_t y = 0u; y < (kan_loop_size_t) mip_height; ++y)
            {
                float *mip_pixel = target_data + image_channels * (x * mip_height + y);
                for (kan_loop_size_t channel = 0u; channel < image_channels; ++channel)
                {
                    switch (preset->mip_generation)
                    {
                    case KAN_RESOURCE_TEXTURE_MIP_GENERATION_AVERAGE:
                        mip_pixel[channel] = 0.0f;
                        break;

                    case KAN_RESOURCE_TEXTURE_MIP_GENERATION_MIN:
                        mip_pixel[channel] = FLT_MAX;
                        break;

                    case KAN_RESOURCE_TEXTURE_MIP_GENERATION_MAX:
                        mip_pixel[channel] = FLT_MIN;
                        break;
                    }
                }

                kan_loop_size_t average_samples = 0u;
                for (kan_loop_size_t offset_x = 0u; offset_x < 2u; ++offset_x)
                {
                    for (kan_loop_size_t offset_y = 0u; offset_y < 2u; ++offset_y)
                    {
                        const kan_loop_size_t sample_x = x * 2u + offset_x;
                        const kan_loop_size_t sample_y = y * 2u + offset_y;

                        if (sample_x < source_width && sample_y < source_height)
                        {
                            const float *sample_pixel =
                                source_data + image_channels * (sample_x * source_height + sample_y);
                            ++average_samples;

                            for (kan_loop_size_t channel = 0u; channel < image_channels; ++channel)
                            {
                                switch (preset->mip_generation)
                                {
                                case KAN_RESOURCE_TEXTURE_MIP_GENERATION_AVERAGE:
                                    mip_pixel[channel] += sample_pixel[channel];
                                    break;

                                case KAN_RESOURCE_TEXTURE_MIP_GENERATION_MIN:
                                    mip_pixel[channel] = KAN_MIN (mip_pixel[channel], sample_pixel[channel]);
                                    break;

                                case KAN_RESOURCE_TEXTURE_MIP_GENERATION_MAX:
                                    mip_pixel[channel] = KAN_MAX (mip_pixel[channel], sample_pixel[channel]);
                                    break;
                                }
                            }
                        }
                    }
                }

                switch (preset->mip_generation)
                {
                case KAN_RESOURCE_TEXTURE_MIP_GENERATION_AVERAGE:
                {
                    KAN_ASSERT (average_samples > 0u)
                    const float average_modifier = 1.0f / (float) average_samples;
                    for (kan_loop_size_t channel = 0u; channel < image_channels; ++channel)
                    {
                        mip_pixel[channel] *= average_modifier;
                    }

                    break;
                }

                case KAN_RESOURCE_TEXTURE_MIP_GENERATION_MIN:
                case KAN_RESOURCE_TEXTURE_MIP_GENERATION_MAX:
                    break;
                }
            }
        }
    }

    struct kan_resource_texture_data_t texture_data;
    kan_allocation_group_stack_push (main_allocation_group);
    kan_resource_texture_data_init (&texture_data);
    kan_allocation_group_stack_pop ();

    CUSHION_DEFER { kan_resource_texture_data_shutdown (&texture_data); }
    bool conversion_successful = true;
    kan_dynamic_array_set_capacity (&output->formats, preset->supported_target_formats.size);

    for (kan_loop_size_t format_index = 0u; format_index < preset->supported_target_formats.size; ++format_index)
    {
        enum kan_resource_texture_format_t format =
            ((enum kan_resource_texture_format_t *) preset->supported_target_formats.data)[format_index];
        bool supported_by_platform = false;

        for (kan_loop_size_t configuration_index = 0u;
             configuration_index < (kan_loop_size_t) configuration->supported_formats.size; ++configuration_index)
        {
            if (format ==
                ((enum kan_resource_texture_format_t *) configuration->supported_formats.data)[configuration_index])
            {
                supported_by_platform = true;
                break;
            }
        }

        if (!supported_by_platform)
        {
            continue;
        }

        // Ensure that target format can be achieved from input image class at all.
        bool format_can_be_targeted = true;

        switch (format)
        {
        case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_R8_SRGB:
        case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_RG16_SRGB:
        case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_RGBA32_SRGB:
        case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_R8_UNORM:
        case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_RG16_UNORM:
        case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_RGBA32_UNORM:
            switch (input->image_class)
            {
            case KAN_RESOURCE_TEXTURE_IMAGE_CLASS_COLOR_SRGB:
            case KAN_RESOURCE_TEXTURE_IMAGE_CLASS_COLOR_RGB:
                KAN_ASSERT (image_channels == 4u)
                break;

            case KAN_RESOURCE_TEXTURE_IMAGE_CLASS_DEPTH_GRAYSCALE:
            case KAN_RESOURCE_TEXTURE_IMAGE_CLASS_DEPTH_FLOAT_32:
                KAN_LOG (resource_render_foundation_texture, KAN_LOG_ERROR,
                         "Texture header \"%s\" declares depth texture, but preset \"%s\" lists color format among "
                         "target formats.",
                         context->primary_name, input->preset)
                format_can_be_targeted = false;
                break;
            }

            break;

        case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_D16:
        case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_D32:
            switch (input->image_class)
            {
            case KAN_RESOURCE_TEXTURE_IMAGE_CLASS_COLOR_SRGB:
            case KAN_RESOURCE_TEXTURE_IMAGE_CLASS_COLOR_RGB:
                KAN_LOG (resource_render_foundation_texture, KAN_LOG_ERROR,
                         "Texture header \"%s\" declares color texture, but preset \"%s\" lists depth format among "
                         "target formats.",
                         context->primary_name, input->preset)
                format_can_be_targeted = false;
                break;

            case KAN_RESOURCE_TEXTURE_IMAGE_CLASS_DEPTH_GRAYSCALE:
            case KAN_RESOURCE_TEXTURE_IMAGE_CLASS_DEPTH_FLOAT_32:
                KAN_ASSERT (image_channels == 1u)
                break;
            }

            break;
        }

        if (!format_can_be_targeted)
        {
            conversion_successful = false;
            continue;
        }

        struct kan_resource_texture_format_item_t *item = kan_dynamic_array_add_last (&output->formats);
        KAN_ASSERT (item)

        kan_allocation_group_stack_push (output->formats.allocation_group);
        kan_resource_texture_format_item_init (item);
        kan_allocation_group_stack_pop ();

        item->format = format;
        kan_dynamic_array_set_capacity (&item->data_per_mip, output->mips);

        for (kan_loop_size_t mip = 0u; mip < (kan_loop_size_t) output->mips; ++mip)
        {
            const kan_instance_size_t width = output->width >> mip;
            const kan_instance_size_t height = output->height >> mip;
            const kan_loop_size_t source_pixel_count = width * height;
            const char *target_format_name = "unknown";

#define CLAMPED_UINT_COLOR(VALUE) (uint8_t) (255.0f * KAN_CLAMP (VALUE, 0.0f, 1.0f))
            switch (format)
            {
            case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_R8_SRGB:
            {
                target_format_name = "r_srgb";
                kan_dynamic_array_set_capacity (&texture_data.data, sizeof (uint8_t) * width * height);
                texture_data.data.size = texture_data.data.capacity;
                const float *source_pixel = image_mips[mip];
                uint8_t *target_pixel = texture_data.data.data;

                for (kan_loop_size_t pixel_index = 0u; pixel_index < source_pixel_count;
                     ++pixel_index, source_pixel += 4u, ++target_pixel)
                {
                    target_pixel[0u] = CLAMPED_UINT_COLOR (kan_color_transfer_rgb_to_srgb (source_pixel[0u]));
                }

                break;
            }

            case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_RG16_SRGB:
            {
                target_format_name = "rg_srgb";
                kan_dynamic_array_set_capacity (&texture_data.data, sizeof (uint16_t) * width * height);
                texture_data.data.size = texture_data.data.capacity;
                const float *source_pixel = image_mips[mip];
                uint8_t *target_pixel = texture_data.data.data;

                for (kan_loop_size_t pixel_index = 0u; pixel_index < source_pixel_count;
                     ++pixel_index, source_pixel += 4u, target_pixel += 2u)
                {
                    target_pixel[0u] = CLAMPED_UINT_COLOR (kan_color_transfer_rgb_to_srgb (source_pixel[0u]));
                    target_pixel[1u] = CLAMPED_UINT_COLOR (kan_color_transfer_rgb_to_srgb (source_pixel[1u]));
                }

                break;
            }

            case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_RGBA32_SRGB:
            {
                target_format_name = "rgba_srgb";
                kan_dynamic_array_set_capacity (&texture_data.data, sizeof (uint32_t) * width * height);
                texture_data.data.size = texture_data.data.capacity;
                const float *source_pixel = image_mips[mip];
                uint8_t *target_pixel = texture_data.data.data;

                for (kan_loop_size_t pixel_index = 0u; pixel_index < source_pixel_count;
                     ++pixel_index, source_pixel += 4u, target_pixel += 4u)
                {
                    target_pixel[0u] = CLAMPED_UINT_COLOR (kan_color_transfer_rgb_to_srgb (source_pixel[0u]));
                    target_pixel[1u] = CLAMPED_UINT_COLOR (kan_color_transfer_rgb_to_srgb (source_pixel[1u]));
                    target_pixel[2u] = CLAMPED_UINT_COLOR (kan_color_transfer_rgb_to_srgb (source_pixel[2u]));
                    target_pixel[3u] = CLAMPED_UINT_COLOR (kan_color_transfer_rgb_to_srgb (source_pixel[3u]));
                }

                break;
            }

            case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_R8_UNORM:
            {
                target_format_name = "r_unorm";
                kan_dynamic_array_set_capacity (&texture_data.data, sizeof (uint8_t) * width * height);
                texture_data.data.size = texture_data.data.capacity;
                const float *source_pixel = image_mips[mip];
                uint8_t *target_pixel = texture_data.data.data;

                for (kan_loop_size_t pixel_index = 0u; pixel_index < source_pixel_count;
                     ++pixel_index, source_pixel += 4u, ++target_pixel)
                {
                    target_pixel[0u] = CLAMPED_UINT_COLOR (source_pixel[0u]);
                }

                break;
            }

            case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_RG16_UNORM:
            {
                target_format_name = "rg_unorm";
                kan_dynamic_array_set_capacity (&texture_data.data, sizeof (uint16_t) * width * height);
                texture_data.data.size = texture_data.data.capacity;
                const float *source_pixel = image_mips[mip];
                uint8_t *target_pixel = texture_data.data.data;

                for (kan_loop_size_t pixel_index = 0u; pixel_index < source_pixel_count;
                     ++pixel_index, source_pixel += 4u, target_pixel += 2u)
                {
                    target_pixel[0u] = CLAMPED_UINT_COLOR (source_pixel[0u]);
                    target_pixel[1u] = CLAMPED_UINT_COLOR (source_pixel[1u]);
                }

                break;
            }

            case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_RGBA32_UNORM:
            {
                target_format_name = "rgba_unorm";
                kan_dynamic_array_set_capacity (&texture_data.data, sizeof (uint32_t) * width * height);
                texture_data.data.size = texture_data.data.capacity;
                const float *source_pixel = image_mips[mip];
                uint8_t *target_pixel = texture_data.data.data;

                for (kan_loop_size_t pixel_index = 0u; pixel_index < source_pixel_count;
                     ++pixel_index, source_pixel += 4u, target_pixel += 4u)
                {
                    target_pixel[0u] = CLAMPED_UINT_COLOR (source_pixel[0u]);
                    target_pixel[1u] = CLAMPED_UINT_COLOR (source_pixel[1u]);
                    target_pixel[2u] = CLAMPED_UINT_COLOR (source_pixel[2u]);
                    target_pixel[3u] = CLAMPED_UINT_COLOR (source_pixel[3u]);
                }

                break;
            }

            case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_D16:
            {
                target_format_name = "d16";
                kan_dynamic_array_set_capacity (&texture_data.data, sizeof (uint16_t) * width * height);
                texture_data.data.size = texture_data.data.capacity;
                const float *source_pixel = image_mips[mip];
                uint16_t *target_pixel = texture_data.data.data;

                for (kan_loop_size_t pixel_index = 0u; pixel_index < source_pixel_count;
                     ++pixel_index, ++source_pixel, ++target_pixel)
                {
                    *target_pixel = (uint16_t) (KAN_CLAMP (*source_pixel, 0.0f, 1.0f) * (float) KAN_INT_MAX (uint16_t));
                }

                break;
            }

            case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_D32:
            {
                target_format_name = "d32";
                kan_dynamic_array_set_capacity (&texture_data.data, sizeof (float) * width * height);
                texture_data.data.size = texture_data.data.capacity;
                memcpy (texture_data.data.data, image_mips[mip], sizeof (float) * width * height);
                break;
            }
            }
#undef CLAMPED_COLOR

            char name_buffer[KAN_RESOURCE_RF_TEXTURE_DATA_MAX_NAME_LENGTH];
            snprintf (name_buffer, sizeof (name_buffer), "%s_%s_mip_%u", context->primary_name, target_format_name,
                      (unsigned int) mip);

            kan_interned_string_t *spot = kan_dynamic_array_add_last (&item->data_per_mip);
            *spot = kan_string_intern (name_buffer);

            if (!context->produce_secondary_output (
                    context->interface, KAN_STATIC_INTERNED_ID_GET (kan_resource_texture_data_t), *spot, &texture_data))
            {
                KAN_LOG (resource_render_foundation_texture, KAN_LOG_ERROR,
                         "Failed to produce data resource \"%s\" for texture \"%s\".", *spot, context->primary_name)
                conversion_successful = false;
            }
        }
    }

    kan_dynamic_array_set_capacity (&output->formats, output->formats.size);
    if (output->formats.size == 0u)
    {
        KAN_LOG (resource_render_foundation_texture, KAN_LOG_ERROR,
                 "Failed to build texture \"%s\" with preset \"%s\": no supported formats found.",
                 context->primary_name, input->preset)
        conversion_successful = false;
    }

    return conversion_successful ? KAN_RESOURCE_BUILD_RULE_SUCCESS : KAN_RESOURCE_BUILD_RULE_FAILURE;
}
