#include <application_framework_example_import_icon_import_api.h>

#include <string.h>

#include <kan/error/critical.h>
#include <kan/image/image.h>
#include <kan/log/logging.h>
#include <kan/resource_pipeline/resource_pipeline.h>

#include <example_import/icon.h>

// \c_interface_scanner_disable
KAN_LOG_DEFINE_CATEGORY (icon_import);
// \c_interface_scanner_enable

struct icon_import_configuration_t
{
    kan_interned_string_t suffix;
};

static kan_bool_t icon_import_functor (struct kan_stream_t *input_stream,
                                       const char *input_path,
                                       kan_reflection_registry_t registry,
                                       void *configuration,
                                       struct kan_resource_import_interface_t *interface)
{
    struct icon_import_configuration_t *import_configuration = configuration;
    const char *last_directory_separator = strrchr (input_path, '/');
    const char *last_dot = strrchr (input_path, '.');

    KAN_ASSERT (last_directory_separator)
    KAN_ASSERT (last_dot)

    const kan_instance_size_t name_length = (kan_instance_size_t) (last_dot - last_directory_separator - 1u);
    const kan_instance_size_t suffix_length = (kan_instance_size_t) strlen (import_configuration->suffix);

#define FILE_NAME_BUFFER_SIZE 1024u
    char file_name_buffer[FILE_NAME_BUFFER_SIZE];
    KAN_ASSERT (name_length + suffix_length + 5u < FILE_NAME_BUFFER_SIZE);
    memcpy (file_name_buffer, last_directory_separator + 1u, name_length);
    memcpy (file_name_buffer + name_length, import_configuration->suffix, suffix_length);
    memcpy (file_name_buffer + name_length + suffix_length, ".bin", 4u);
    file_name_buffer[name_length + suffix_length + 4u] = '\0';

    struct kan_image_raw_data_t raw_data;
    kan_image_raw_data_init (&raw_data);

    if (!kan_image_load (input_stream, &raw_data))
    {
        KAN_LOG (icon_import, KAN_LOG_ERROR, "Failed to import image from \"%s\".", input_path)
        kan_image_raw_data_shutdown (&raw_data);
        return KAN_FALSE;
    }

    struct icon_t icon;
    icon_init (&icon);

    icon.width = (kan_serialized_size_t) raw_data.width;
    icon.height = (kan_serialized_size_t) raw_data.height;

    kan_dynamic_array_set_capacity (&icon.pixels, icon.width * icon.height);
    icon.pixels.size = icon.width * icon.height;
    memcpy (icon.pixels.data, raw_data.data, icon.width * icon.height * sizeof (rgba_pixel_t));

    kan_bool_t result =
        interface->produce (interface->user_data, file_name_buffer, kan_string_intern ("icon_t"), &icon);

    icon_shutdown (&icon);
    kan_image_raw_data_shutdown (&raw_data);
    return result;
}

// \meta reflection_struct_meta = "icon_import_configuration_t"
APPLICATION_FRAMEWORK_EXAMPLE_IMPORT_ICON_IMPORT_API struct kan_resource_import_configuration_type_meta_t meta = {
    .functor = icon_import_functor,
    .allow_checksum = KAN_TRUE,
};
