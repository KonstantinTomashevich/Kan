#pragma once

#include <resource_texture_import_api.h>

#include <kan/resource_texture/resource_texture.h>

/// \file
/// \brief Provides import config type for texture import using image interface.
///
/// \par Overview
/// \parblock
/// As texture import is configuration+tool based, this file is only needed to store documentation for texture import.
///
/// This texture import implementation uses image abstract unit to read data from well known image formats and produce
/// `kan_resource_texture_raw_data_t` from it with the same name as imported image (excluding file extension).
/// \endparblock

/// \brief Import configuration for importing raw texture data from images.
struct kan_resource_texture_import_config_t
{
    /// \brief Raw format to which imported texture data should be converted.
    /// \details Useful for stripping off unnecessary data, for example for
    ///          stripping excessive channels from grayscale texture.
    enum kan_resource_texture_raw_format_t target_raw_format;
};
