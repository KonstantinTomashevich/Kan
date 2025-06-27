#include <string.h>

#include <kan/container/trivial_string_buffer.h>
#include <kan/image/image.h>
#include <kan/log/logging.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/resource_texture_import/resource_texture_import.h>

KAN_LOG_DEFINE_CATEGORY (resource_texture_import);

static bool resource_texture_import_functor (struct kan_stream_t *input_stream,
                                             const char *input_path,
                                             kan_reflection_registry_t registry,
                                             void *configuration,
                                             struct kan_resource_import_interface_t *interface)
{
    const struct kan_resource_texture_import_config_t *config = configuration;
    kan_allocation_group_t allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "resource_texture_import");

    struct kan_image_raw_data_t image_data;
    kan_image_raw_data_init (&image_data);

    if (!kan_image_load (input_stream, &image_data))
    {
        KAN_LOG (resource_texture_import, KAN_LOG_ERROR, "Failed to import image from \"%s\".", input_path)
        kan_image_raw_data_shutdown (&image_data);
        return false;
    }

    struct kan_trivial_string_buffer_t relative_path_buffer;
    kan_trivial_string_buffer_init (&relative_path_buffer, allocation_group, 256u);

    const char *file_name_begin;
    const char *file_name_end;
    kan_resource_import_extract_file_name (input_path, &file_name_begin, &file_name_end);
    kan_trivial_string_buffer_append_char_sequence (&relative_path_buffer, file_name_begin,
                                                    (kan_instance_size_t) (file_name_end - file_name_begin));
    kan_trivial_string_buffer_append_string (&relative_path_buffer, ".bin");

    bool successful = true;
    struct kan_resource_texture_raw_data_t raw_data;
    kan_resource_texture_raw_data_init (&raw_data);

    raw_data.width = image_data.width;
    raw_data.height = image_data.height;
    raw_data.depth = 1u;
    raw_data.format = config->target_raw_format;

#define COPY_WITH_STRIPPING_CHANNELS(CHANNELS)                                                                         \
    {                                                                                                                  \
        kan_dynamic_array_set_capacity (&raw_data.data, raw_data.width *raw_data.height *CHANNELS);                    \
        raw_data.data.size = raw_data.data.capacity;                                                                   \
        const uint8_t *input_data = (const uint8_t *) image_data.data;                                                 \
        uint8_t *output_data = raw_data.data.data;                                                                     \
        const kan_loop_size_t pixel_count = raw_data.width * raw_data.height;                                          \
                                                                                                                       \
        for (kan_loop_size_t pixel_index = 0u; pixel_index < pixel_count; ++pixel_index)                               \
        {                                                                                                              \
            const uint8_t *pixel_input = input_data + pixel_index * 4u;                                                \
            uint8_t *pixel_output = output_data + pixel_index * CHANNELS;                                              \
                                                                                                                       \
            for (kan_loop_size_t channel = 0u; channel < CHANNELS; ++channel)                                          \
            {                                                                                                          \
                pixel_output[channel] = pixel_input[channel];                                                          \
            }                                                                                                          \
        }                                                                                                              \
    }

    // Currently, image interface provides no data on whether it is SRGB or UNORM, therefore we just trust the user.
    switch (config->target_raw_format)
    {
    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_R8_SRGB:
    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_R8_UNORM:
        COPY_WITH_STRIPPING_CHANNELS (1u)
        break;

    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RG16_SRGB:
    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RG16_UNORM:
        COPY_WITH_STRIPPING_CHANNELS (2u)
        break;

    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGBA32_SRGB:
    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGBA32_UNORM:
        kan_dynamic_array_set_capacity (&raw_data.data, raw_data.width * raw_data.height * 4u);
        raw_data.data.size = raw_data.data.capacity;
        memcpy (raw_data.data.data, image_data.data, raw_data.data.size);
        break;

    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH16:
    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH32:
        KAN_LOG (resource_texture_import, KAN_LOG_ERROR, "Depth texture import is not yet supported.")
        successful = false;
        break;
    }

#undef COPY_WITH_STRIPPING_CHANNELS

    if (!interface->produce (interface->user_data, relative_path_buffer.buffer,
                             kan_string_intern ("kan_resource_texture_raw_data_t"), &raw_data))
    {
        KAN_LOG (resource_texture_import, KAN_LOG_ERROR, "Failed to produce raw texture data resource.")
        successful = false;
    }

    kan_resource_texture_raw_data_shutdown (&raw_data);
    kan_trivial_string_buffer_shutdown (&relative_path_buffer);
    kan_image_raw_data_shutdown (&image_data);
    return successful;
}

KAN_REFLECTION_STRUCT_META (kan_resource_texture_import_config_t)
RESOURCE_TEXTURE_IMPORT_API struct kan_resource_import_configuration_type_meta_t resource_texture_import_meta = {
    .functor = resource_texture_import_functor,
    .allow_checksum = true,
};
