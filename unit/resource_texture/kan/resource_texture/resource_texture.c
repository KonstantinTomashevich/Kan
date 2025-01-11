#include <string.h>

#include <kan/log/logging.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/resource_texture/resource_texture.h>

KAN_LOG_DEFINE_CATEGORY (resource_texture_compilation);

KAN_REFLECTION_STRUCT_META (kan_resource_texture_raw_data_t)
RESOURCE_TEXTURE_API struct kan_resource_resource_type_meta_t kan_resource_texture_raw_data_resource_type_meta = {
    .root = KAN_FALSE,
};

KAN_REFLECTION_STRUCT_META (kan_resource_texture_compilation_preset_t)
RESOURCE_TEXTURE_API struct kan_resource_resource_type_meta_t
    kan_resource_texture_compilation_preset_resource_type_meta = {
        .root = KAN_FALSE,
};

KAN_REFLECTION_STRUCT_META (kan_resource_texture_t)
RESOURCE_TEXTURE_API struct kan_resource_resource_type_meta_t kan_resource_texture_resource_type_meta = {
    .root = KAN_FALSE,
};

static enum kan_resource_compile_result_t kan_resource_texture_compile (struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (kan_resource_texture_t)
RESOURCE_TEXTURE_API struct kan_resource_compilable_meta_t kan_resource_texture_compilable_meta = {
    .output_type_name = "kan_resource_texture_compiled_t",
    .configuration_type_name = "kan_resource_texture_platform_configuration_t",
    // No state as textures should never be compiled in runtime.
    .state_type_name = NULL,
    .functor = kan_resource_texture_compile,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_texture_t, raw_data)
RESOURCE_TEXTURE_API struct kan_resource_reference_meta_t kan_resource_texture_raw_data_reference_meta = {
    .type = "kan_resource_texture_raw_data_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_texture_t, compilation_preset)
RESOURCE_TEXTURE_API struct kan_resource_reference_meta_t kan_resource_texture_compilation_preset_reference_meta = {
    .type = "kan_resource_texture_compilation_preset_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW,
};

KAN_REFLECTION_STRUCT_META (kan_resource_texture_compiled_data_t)
RESOURCE_TEXTURE_API struct kan_resource_byproduct_type_meta_t kan_resource_texture_compiled_data_byproduct_type_meta =
    {
        .hash = kan_resource_byproduct_hash_unique,
        .is_equal = kan_resource_byproduct_is_equal_unique,
        .move = NULL,
        .reset = NULL,
};

KAN_REFLECTION_STRUCT_META (kan_resource_texture_compiled_t)
RESOURCE_TEXTURE_API struct kan_resource_resource_type_meta_t kan_resource_texture_compiled_resource_type_meta = {
    .root = KAN_FALSE,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_texture_compiled_format_item_t, compiled_data_per_mip)
RESOURCE_TEXTURE_API struct kan_resource_reference_meta_t
    kan_resource_texture_compiled_format_item_compiled_data_per_mip_reference_meta = {
        .type = "kan_resource_texture_compiled_data_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

static enum kan_resource_compile_result_t kan_resource_texture_compile (struct kan_resource_compile_state_t *state)
{
    const struct kan_resource_texture_t *input = state->input_instance;
    struct kan_resource_texture_compiled_t *output = state->output_instance;
    const struct kan_resource_texture_platform_configuration_t *configuration = state->platform_configuration;

    const struct kan_resource_texture_raw_data_t *raw_data = NULL;
    const struct kan_resource_texture_compilation_preset_t *preset = NULL;

    const kan_interned_string_t interned_kan_resource_texture_raw_data_t =
        kan_string_intern ("kan_resource_texture_raw_data_t");
    const kan_interned_string_t interned_kan_resource_texture_compilation_preset_t =
        kan_string_intern ("kan_resource_texture_compilation_preset_t");
    const kan_interned_string_t interned_kan_resource_texture_compiled_data_t =
        kan_string_intern ("kan_resource_texture_compiled_data_t");

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) state->dependencies_count; ++index)
    {
        if (state->dependencies[index].type == interned_kan_resource_texture_raw_data_t)
        {
            raw_data = state->dependencies[index].data;
        }
        else if (state->dependencies[index].type == interned_kan_resource_texture_compilation_preset_t)
        {
            preset = state->dependencies[index].data;
        }
    }

    if (!raw_data)
    {
        KAN_LOG (resource_texture_compilation, KAN_LOG_ERROR, "Texture \"%s\" has no raw data!", state->name)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    if (!preset)
    {
        KAN_LOG (resource_texture_compilation, KAN_LOG_ERROR, "Texture \"%s\" has no compilation preset!", state->name)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    output->width = raw_data->width;
    output->height = raw_data->height;
    output->depth = raw_data->depth;
    output->mips = input->mips;
    kan_dynamic_array_set_capacity (&output->compiled_formats, preset->supported_compiled_formats.size);

    if (output->width == 0u)
    {
        KAN_LOG (resource_texture_compilation, KAN_LOG_ERROR, "Texture \"%s\" has zero width, which is not supported!",
                 state->name)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    if (output->height == 0u)
    {
        KAN_LOG (resource_texture_compilation, KAN_LOG_ERROR, "Texture \"%s\" has zero height, which is not supported!",
                 state->name)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    if (output->depth == 0u)
    {
        KAN_LOG (resource_texture_compilation, KAN_LOG_ERROR, "Texture \"%s\" has zero depth, which is not supported!",
                 state->name)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    if (output->mips == 0u)
    {
        KAN_LOG (resource_texture_compilation, KAN_LOG_ERROR, "Texture \"%s\" has zero mips, which is not supported!",
                 state->name)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    kan_bool_t successful = KAN_TRUE;
    kan_allocation_group_t main_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "texture_compilation");
    kan_allocation_group_t mips_allocation_group = kan_allocation_group_get_child (main_allocation_group, "mips");

    kan_instance_size_t raw_pixel_size = 1u;
    switch (raw_data->format)
    {
    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_R8:
        raw_pixel_size = 1u;
        break;

    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RG16:
    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH16:
        raw_pixel_size = 2u;
        break;

    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGB24:
        raw_pixel_size = 3u;
        break;

    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGBA32:
    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH32:
        raw_pixel_size = 4u;
        break;
    }

    void **generated_mips_data = NULL;
    if (output->mips > 1u)
    {
        generated_mips_data =
            kan_allocate_general (mips_allocation_group, sizeof (void *) * (output->mips - 1u), _Alignof (void *));

        for (kan_loop_size_t mip = 1u; mip < (kan_loop_size_t) output->mips; ++mip)
        {
            generated_mips_data[mip - 1u] = NULL;
        }

        for (kan_loop_size_t mip = 1u; mip < (kan_loop_size_t) output->mips; ++mip)
        {
            const kan_instance_size_t source_width = output->width > 1u ? (output->width >> (mip - 1u)) : 1u;
            const kan_instance_size_t source_height = output->height > 1u ? (output->height >> (mip - 1u)) : 1u;
            const kan_instance_size_t source_depth = output->depth > 1u ? (output->depth >> (mip - 1u)) : 1u;

            const kan_instance_size_t mip_width = output->width > 1u ? (output->width >> mip) : 1u;
            const kan_instance_size_t mip_height = output->height > 1u ? (output->height >> mip) : 1u;
            const kan_instance_size_t mip_depth = output->depth > 1u ? (output->depth >> mip) : 1u;

            generated_mips_data[mip - 1u] = kan_allocate_general (
                mips_allocation_group, raw_pixel_size * mip_width * mip_height * mip_depth, _Alignof (uint8_t));

            const uint8_t *source_data = mip > 1u ? generated_mips_data[mip - 2u] : raw_data->data.data;
            uint8_t *target_data = generated_mips_data[mip - 1u];

#define GENERATE_MIP(CHANNELS, CHANNEL_TYPE, CHANNEL_SUM_TYPE)                                                         \
    for (kan_loop_size_t x = 0u; x < (kan_loop_size_t) mip_width; ++x)                                                 \
    {                                                                                                                  \
        for (kan_loop_size_t y = 0u; y < (kan_loop_size_t) mip_height; ++y)                                            \
        {                                                                                                              \
            for (kan_loop_size_t z = 0u; z < mip_depth; ++z)                                                           \
            {                                                                                                          \
                kan_loop_size_t samples = 0u;                                                                          \
                CHANNEL_SUM_TYPE sums[CHANNELS];                                                                       \
                                                                                                                       \
                for (kan_loop_size_t channel = 0u; channel < CHANNELS; ++channel)                                      \
                {                                                                                                      \
                    sums[channel] = (CHANNEL_SUM_TYPE) 0;                                                              \
                }                                                                                                      \
                                                                                                                       \
                for (kan_loop_size_t offset_x = 0u; offset_x < 2u; ++offset_x)                                         \
                {                                                                                                      \
                    for (kan_loop_size_t offset_y = 0u; offset_y < 2u; ++offset_y)                                     \
                    {                                                                                                  \
                        for (kan_loop_size_t offset_z = 0u; offset_z < 2u; ++offset_z)                                 \
                        {                                                                                              \
                            const kan_loop_size_t sample_x = x * 2u + offset_x;                                        \
                            const kan_loop_size_t sample_y = y * 2u + offset_y;                                        \
                            const kan_loop_size_t sample_z = z * 2u + offset_z;                                        \
                                                                                                                       \
                            if (sample_x < source_width && sample_y < source_height && sample_z < source_depth)        \
                            {                                                                                          \
                                ++samples;                                                                             \
                                for (kan_loop_size_t channel = 0u; channel < CHANNELS; ++channel)                      \
                                {                                                                                      \
                                    const kan_memory_size_t pixel_index =                                              \
                                        sample_x * source_height * source_depth + sample_y * source_depth + sample_z;  \
                                                                                                                       \
                                    const kan_memory_size_t offset =                                                   \
                                        pixel_index * raw_pixel_size + sizeof (CHANNEL_TYPE) * channel;                \
                                                                                                                       \
                                    sums[channel] += *(CHANNEL_TYPE *) &source_data[offset];                           \
                                }                                                                                      \
                            }                                                                                          \
                        }                                                                                              \
                    }                                                                                                  \
                }                                                                                                      \
                                                                                                                       \
                KAN_ASSERT (samples > 0u)                                                                              \
                for (kan_loop_size_t channel = 0u; channel < CHANNELS; ++channel)                                      \
                {                                                                                                      \
                    const kan_memory_size_t pixel_index = x * mip_height * mip_depth + y * mip_depth + z;              \
                                                                                                                       \
                    const kan_memory_size_t offset = pixel_index * raw_pixel_size + sizeof (CHANNEL_TYPE) * channel;   \
                                                                                                                       \
                    *(CHANNEL_TYPE *) &target_data[offset] =                                                           \
                        (CHANNEL_TYPE) (sums[channel] / (CHANNEL_SUM_TYPE) samples);                                   \
                }                                                                                                      \
            }                                                                                                          \
        }                                                                                                              \
    }

            switch (raw_data->format)
            {
            case KAN_RESOURCE_TEXTURE_RAW_FORMAT_R8:
                GENERATE_MIP (1u, uint8_t, kan_loop_size_t)
                break;

            case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RG16:
                GENERATE_MIP (2u, uint8_t, kan_loop_size_t)
                break;

            case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGB24:
                GENERATE_MIP (3u, uint8_t, kan_loop_size_t)
                break;

            case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGBA32:
                GENERATE_MIP (4u, uint8_t, kan_loop_size_t)
                break;

            case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH16:
                GENERATE_MIP (1u, uint16_t, kan_loop_size_t)
                break;

            case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH32:
                GENERATE_MIP (1u, float, double)
                break;
            }

#undef GENERATE_MIP
        }
    }

    if (successful)
    {
        struct kan_resource_texture_compiled_data_t compiled_data;
        kan_allocation_group_stack_push (main_allocation_group);
        kan_resource_texture_compiled_data_init (&compiled_data);
        kan_allocation_group_stack_pop ();

        for (kan_loop_size_t preset_index = 0u;
             preset_index < (kan_loop_size_t) preset->supported_compiled_formats.size && successful; ++preset_index)
        {
            enum kan_resource_texture_compiled_format_t format =
                ((enum kan_resource_texture_compiled_format_t *) preset->supported_compiled_formats.data)[preset_index];
            kan_bool_t supported_by_platform = KAN_FALSE;

            for (kan_loop_size_t configuration_index = 0u;
                 configuration_index < (kan_loop_size_t) configuration->supported_compiled_formats.size;
                 ++configuration_index)
            {
                if (format == ((enum kan_resource_texture_compiled_format_t *)
                                   configuration->supported_compiled_formats.data)[configuration_index])
                {
                    supported_by_platform = KAN_TRUE;
                    break;
                }
            }

            if (!supported_by_platform)
            {
                continue;
            }

            struct kan_resource_texture_compiled_format_item_t *item =
                kan_dynamic_array_add_last (&output->compiled_formats);
            KAN_ASSERT (item)
            item->format = format;
            kan_dynamic_array_set_capacity (&item->compiled_data_per_mip, output->mips);

            for (kan_loop_size_t mip = 0u; mip < (kan_loop_size_t) output->mips; ++mip)
            {
                const uint8_t *compression_input = mip > 0u ? generated_mips_data[mip - 1u] : raw_data->data.data;
                const kan_instance_size_t width = output->width > 1u ? (output->width >> mip) : 1u;
                const kan_instance_size_t height = output->height > 1u ? (output->height >> mip) : 1u;
                const kan_instance_size_t depth = output->depth > 1u ? (output->depth >> mip) : 1u;

#define COPY_CHANNELS_SAME_COUNT(CHANNEL_TYPE, CHANNELS)                                                               \
    {                                                                                                                  \
        const kan_instance_size_t pixel_count = width * height * depth;                                                \
        kan_dynamic_array_set_capacity (&compiled_data.data, pixel_count * CHANNELS * sizeof (CHANNEL_TYPE));          \
        compiled_data.data.size = compiled_data.data.capacity;                                                         \
        uint8_t *compression_output = (uint8_t *) compiled_data.data.data;                                             \
        memcpy (compression_output, compression_input, compiled_data.data.size);                                       \
    }

#define COPY_CHANNELS_DIFFERENT_COUNT(CHANNEL_TYPE, INPUT_CHANNELS, OUTPUT_CHANNELS)                                   \
    {                                                                                                                  \
        const kan_instance_size_t pixel_count = width * height * depth;                                                \
        kan_dynamic_array_set_capacity (&compiled_data.data, pixel_count * OUTPUT_CHANNELS * sizeof (CHANNEL_TYPE));   \
        compiled_data.data.size = pixel_count * compiled_data.data.capacity;                                           \
        uint8_t *compression_output = (uint8_t *) compiled_data.data.data;                                             \
                                                                                                                       \
        for (kan_loop_size_t pixel_index = 0u; pixel_index < (kan_loop_size_t) pixel_count; ++pixel_index)             \
        {                                                                                                              \
            CHANNEL_TYPE *pixel_input =                                                                                \
                (CHANNEL_TYPE *) (compression_input + pixel_index * INPUT_CHANNELS * sizeof (CHANNEL_TYPE));           \
            CHANNEL_TYPE *pixel_output =                                                                               \
                (CHANNEL_TYPE *) (compression_output + pixel_index * OUTPUT_CHANNELS * sizeof (CHANNEL_TYPE));         \
                                                                                                                       \
            for (kan_loop_size_t channel = 0u; channel < OUTPUT_CHANNELS; ++channel)                                   \
            {                                                                                                          \
                pixel_output[channel] = pixel_input[channel];                                                          \
            }                                                                                                          \
        }                                                                                                              \
    }

                switch (format)
                {
                case KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_R8:
                    switch (raw_data->format)
                    {
                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_R8:
                        COPY_CHANNELS_SAME_COUNT (uint8_t, 1u)
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RG16:
                        COPY_CHANNELS_DIFFERENT_COUNT (uint8_t, 2u, 1u)
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGB24:
                        COPY_CHANNELS_DIFFERENT_COUNT (uint8_t, 3u, 1u)
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGBA32:
                        COPY_CHANNELS_DIFFERENT_COUNT (uint8_t, 4u, 1u)
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH16:
                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH32:
                        KAN_LOG (resource_texture_compilation, KAN_LOG_ERROR,
                                 "Cannot convert texture \"%s\" from raw depth format to color format.\n", state->name)
                        successful = KAN_FALSE;
                        break;
                    }

                    break;

                case KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_RG16:
                    switch (raw_data->format)
                    {
                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_R8:
                        KAN_LOG (
                            resource_texture_compilation, KAN_LOG_ERROR,
                            "Cannot convert texture \"%s\" from 1-channel color to 2-channel color (inefficient).\n",
                            state->name)
                        successful = KAN_FALSE;
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RG16:
                        COPY_CHANNELS_SAME_COUNT (uint8_t, 2u)
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGB24:
                        COPY_CHANNELS_DIFFERENT_COUNT (uint8_t, 3u, 2u)
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGBA32:
                        COPY_CHANNELS_DIFFERENT_COUNT (uint8_t, 4u, 2u)
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH16:
                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH32:
                        KAN_LOG (resource_texture_compilation, KAN_LOG_ERROR,
                                 "Cannot convert texture \"%s\" from raw depth format to color format.\n", state->name)
                        successful = KAN_FALSE;
                        break;
                    }

                    break;

                case KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_RGB24:
                    switch (raw_data->format)
                    {
                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_R8:
                        KAN_LOG (
                            resource_texture_compilation, KAN_LOG_ERROR,
                            "Cannot convert texture \"%s\" from 1-channel color to 3-channel color (inefficient).\n",
                            state->name)
                        successful = KAN_FALSE;
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RG16:
                        KAN_LOG (
                            resource_texture_compilation, KAN_LOG_ERROR,
                            "Cannot convert texture \"%s\" from 2-channel color to 3-channel color (inefficient).\n",
                            state->name)
                        successful = KAN_FALSE;
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGB24:
                        COPY_CHANNELS_SAME_COUNT (uint8_t, 3u)
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGBA32:
                        COPY_CHANNELS_DIFFERENT_COUNT (uint8_t, 4u, 3u)
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH16:
                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH32:
                        KAN_LOG (resource_texture_compilation, KAN_LOG_ERROR,
                                 "Cannot convert texture \"%s\" from raw depth format to color format.\n", state->name)
                        successful = KAN_FALSE;
                        break;
                    }

                    break;

                case KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_RGBA32:
                    switch (raw_data->format)
                    {
                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_R8:
                        KAN_LOG (
                            resource_texture_compilation, KAN_LOG_ERROR,
                            "Cannot convert texture \"%s\" from 1-channel color to 4-channel color (inefficient).\n",
                            state->name)
                        successful = KAN_FALSE;
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RG16:
                        KAN_LOG (
                            resource_texture_compilation, KAN_LOG_ERROR,
                            "Cannot convert texture \"%s\" from 2-channel color to 4-channel color (inefficient).\n",
                            state->name)
                        successful = KAN_FALSE;
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGB24:
                        KAN_LOG (
                            resource_texture_compilation, KAN_LOG_ERROR,
                            "Cannot convert texture \"%s\" from 3-channel color to 4-channel color (inefficient).\n",
                            state->name)
                        successful = KAN_FALSE;
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGBA32:
                        COPY_CHANNELS_SAME_COUNT (uint8_t, 4u)
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH16:
                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH32:
                        KAN_LOG (resource_texture_compilation, KAN_LOG_ERROR,
                                 "Cannot convert texture \"%s\" from raw depth format to color format.\n", state->name)
                        successful = KAN_FALSE;
                        break;
                    }

                    break;

                case KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_D16:
                    switch (raw_data->format)
                    {
                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_R8:
                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RG16:
                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGB24:
                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGBA32:
                        KAN_LOG (resource_texture_compilation, KAN_LOG_ERROR,
                                 "Cannot convert texture \"%s\" from raw color format to depth format.\n", state->name)
                        successful = KAN_FALSE;
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH16:
                        COPY_CHANNELS_SAME_COUNT (uint16_t, 1u)
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH32:
                    {
                        const kan_instance_size_t pixel_count = width * height * depth;
                        kan_dynamic_array_set_capacity (&compiled_data.data, pixel_count * sizeof (uint16_t));
                        compiled_data.data.size = pixel_count * compiled_data.data.capacity;
                        uint8_t *compression_output = (uint8_t *) compiled_data.data.data;

                        for (kan_loop_size_t pixel_index = 0u; pixel_index < (kan_loop_size_t) pixel_count;
                             ++pixel_index)
                        {
                            float *pixel_input = (float *) (compression_input + pixel_index * sizeof (float));
                            const float normalized_input = KAN_MIN (KAN_MAX (*pixel_input, 0.0f), 1.0f);

                            uint16_t *pixel_output =
                                (uint16_t *) (compression_output + pixel_index * sizeof (uint16_t));
                            *pixel_output = (uint16_t) (normalized_input * (float) KAN_INT_MAX (uint16_t));
                        }

                        break;
                    }
                    }

                    break;

                case KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_D32:
                    switch (raw_data->format)
                    {
                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_R8:
                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RG16:
                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGB24:
                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGBA32:
                        KAN_LOG (resource_texture_compilation, KAN_LOG_ERROR,
                                 "Cannot convert texture \"%s\" from raw color format to depth format.\n", state->name)
                        successful = KAN_FALSE;
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH16:
                        KAN_LOG (resource_texture_compilation, KAN_LOG_ERROR,
                                 "Cannot convert texture \"%s\" from 16-bit depth to 32-bit depth (inefficient).\n",
                                 state->name)
                        successful = KAN_FALSE;
                        break;

                    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH32:
                        COPY_CHANNELS_SAME_COUNT (float, 1u)
                        break;
                    }

                    break;
                }

#undef COPY_CHANNELS_SAME_COUNT
#undef COPY_CHANNELS_DIFFERENT_COUNT

                if (successful)
                {
                    kan_interned_string_t *spot = kan_dynamic_array_add_last (&item->compiled_data_per_mip);
                    KAN_ASSERT (spot)
                    *spot = state->register_byproduct (state->interface_user_data,
                                                       interned_kan_resource_texture_compiled_data_t, &compiled_data);
                }
            }
        }

        kan_resource_texture_compiled_data_shutdown (&compiled_data);
    }

    if (generated_mips_data)
    {
        for (kan_loop_size_t mip = 1u; mip < (kan_loop_size_t) output->mips; ++mip)
        {
            const kan_instance_size_t width = output->width > 1u ? (output->width >> mip) : 1u;
            const kan_instance_size_t height = output->height > 1u ? (output->height >> mip) : 1u;
            const kan_instance_size_t depth = output->depth > 1u ? (output->depth >> mip) : 1u;
            kan_free_general (mips_allocation_group, generated_mips_data[mip - 1u],
                              raw_pixel_size * width * height * depth);
        }

        kan_free_general (mips_allocation_group, generated_mips_data, sizeof (void *) * (output->mips - 1u));
    }

    kan_dynamic_array_set_capacity (&output->compiled_formats, output->compiled_formats.size);
    return successful ? KAN_RESOURCE_PIPELINE_COMPILE_FINISHED : KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
}

void kan_resource_texture_raw_data_init (struct kan_resource_texture_raw_data_t *instance)
{
    instance->width = 1u;
    instance->height = 1u;
    instance->depth = 1u;
    instance->format = KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGBA32;

    kan_dynamic_array_init (&instance->data, 0u, sizeof (uint8_t), _Alignof (uint8_t),
                            kan_allocation_group_stack_get ());
}

void kan_resource_texture_raw_data_shutdown (struct kan_resource_texture_raw_data_t *instance)
{
    kan_dynamic_array_shutdown (&instance->data);
}

void kan_resource_texture_compilation_preset_init (struct kan_resource_texture_compilation_preset_t *instance)
{
    kan_dynamic_array_init (&instance->supported_compiled_formats, 0u,
                            sizeof (enum kan_resource_texture_compiled_format_t),
                            _Alignof (enum kan_resource_texture_compiled_format_t), kan_allocation_group_stack_get ());
}

void kan_resource_texture_compilation_preset_shutdown (struct kan_resource_texture_compilation_preset_t *instance)
{
    kan_dynamic_array_shutdown (&instance->supported_compiled_formats);
}

void kan_resource_texture_init (struct kan_resource_texture_t *instance)
{
    instance->raw_data = NULL;
    instance->compilation_preset = NULL;
    instance->mips = 1u;
}

void kan_resource_texture_platform_configuration_init (struct kan_resource_texture_platform_configuration_t *instance)
{
    kan_dynamic_array_init (&instance->supported_compiled_formats, 0u,
                            sizeof (enum kan_resource_texture_compiled_format_t),
                            _Alignof (enum kan_resource_texture_compiled_format_t), kan_allocation_group_stack_get ());
}

void kan_resource_texture_platform_configuration_shutdown (
    struct kan_resource_texture_platform_configuration_t *instance)
{
    kan_dynamic_array_shutdown (&instance->supported_compiled_formats);
}

void kan_resource_texture_compiled_data_init (struct kan_resource_texture_compiled_data_t *instance)
{
    kan_dynamic_array_init (&instance->data, 0u, sizeof (uint8_t), _Alignof (uint8_t),
                            kan_allocation_group_stack_get ());
}

void kan_resource_texture_compiled_data_shutdown (struct kan_resource_texture_compiled_data_t *instance)
{
    kan_dynamic_array_shutdown (&instance->data);
}

void kan_resource_texture_compiled_format_item_init (struct kan_resource_texture_compiled_format_item_t *instance)
{
    instance->format = KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_R8;
    kan_dynamic_array_init (&instance->compiled_data_per_mip, 0u, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), kan_allocation_group_stack_get ());
}

void kan_resource_texture_compiled_format_item_shutdown (struct kan_resource_texture_compiled_format_item_t *instance)
{
    kan_dynamic_array_shutdown (&instance->compiled_data_per_mip);
}

void kan_resource_texture_compiled_init (struct kan_resource_texture_compiled_t *instance)
{
    instance->width = 1u;
    instance->height = 1u;
    instance->depth = 1u;
    instance->mips = 1u;

    kan_dynamic_array_init (
        &instance->compiled_formats, 0u, sizeof (struct kan_resource_texture_compiled_format_item_t),
        _Alignof (struct kan_resource_texture_compiled_format_item_t), kan_allocation_group_stack_get ());
}

void kan_resource_texture_compiled_shutdown (struct kan_resource_texture_compiled_t *instance)
{
    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) instance->compiled_formats.size; ++index)
    {
        kan_resource_texture_compiled_format_item_shutdown (
            &((struct kan_resource_texture_compiled_format_item_t *) instance->compiled_formats.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->compiled_formats);
}
