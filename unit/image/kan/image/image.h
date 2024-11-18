#pragma once

#include <image_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/stream/stream.h>

/// \file
/// \brief Provides simplistic API for decoding and encoding image data.
///
/// \par Description
/// \parblock
/// Provided API supports loading and saving pixel data for PNG, TGA and BMP image formats.
/// This API should only be used in tools and tests and should never enter application runtime code as it is not
/// well suited for applications. Majorly, because it does not support incremental operations at all.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Contains information about image size and data pointer if loaded successfully.
struct kan_image_raw_data_t
{
    kan_instance_size_t width;
    kan_instance_size_t height;

    /// \brief Image data in RGBA format.
    uint8_t *data;
};

IMAGE_API void kan_image_raw_data_init (struct kan_image_raw_data_t *data);

IMAGE_API void kan_image_raw_data_shutdown (struct kan_image_raw_data_t *data);

/// \brief Attempts to load PNG, TGA or BMP image from given stream.
IMAGE_API kan_bool_t kan_image_load (struct kan_stream_t *stream, struct kan_image_raw_data_t *output);

enum kan_image_save_format_t
{
    KAN_IMAGE_SAVE_FORMAT_PNG = 0u,
    KAN_IMAGE_SAVE_FORMAT_TGA,
    KAN_IMAGE_SAVE_FORMAT_BMP,
};

/// \brief Attempts to save PNG, TGA or BMP image to given stream.
IMAGE_API kan_bool_t kan_image_save (struct kan_stream_t *stream,
                                     enum kan_image_save_format_t format,
                                     struct kan_image_raw_data_t *input);

KAN_C_HEADER_END
