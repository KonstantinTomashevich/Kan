#pragma once

#include <resource_locale_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/error/critical.h>
#include <kan/reflection/markup.h>

/// \file
/// \brief This file stores representation of locale configuration resources.
///
/// \par Overview
/// \parblock
/// Locales are used to represent full localization set for particular region. Languages store configuration for one
/// particular language. One locale can use several languages if game text needs it, for example russian as main text
/// language and english for some particular important keywords. That is the reason why locales and languages were
/// separated.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Which text orientation is preferred for this locale.
/// \details While most modern locales prefer horizontal text layout, 
///          some historically styled locales might prefer vertical layout for their text.
enum kan_locale_preferred_orientation_t
{
    KAN_LOCALE_PREFERRED_ORIENTATION_HORIZONTAL = 0u,
    KAN_LOCALE_PREFERRED_ORIENTATION_VERTICAL,
};

/// \brief Describes information about particular locale for the application.
struct kan_resource_locale_t
{
    enum kan_locale_preferred_orientation_t preferred_orientation;

    /// \brief When this locale is selected and this array is not empty,
    ///        font libraries should only load font faces for these languages.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t font_language_filter;
};

RESOURCE_LOCALE_API void kan_resource_locale_init (struct kan_resource_locale_t *instance);

RESOURCE_LOCALE_API void kan_resource_locale_shutdown (struct kan_resource_locale_t *instance);

/// \brief Directions for horizontal text.
enum kan_language_horizontal_direction_t
{
    KAN_LANGUAGE_HORIZONTAL_DIRECTION_LEFT_TO_RIGHT = 0u,
    KAN_LANGUAGE_HORIZONTAL_DIRECTION_RIGHT_TO_LEFT,
};

/// \brief Directions for vertical text.
enum kan_language_vertical_direction_t
{
    KAN_LANGUAGE_VERTICAL_DIRECTION_TOP_TO_BOTTOM = 0u,
    KAN_LANGUAGE_VERTICAL_DIRECTION_BOTTOM_TO_TOP,
};

/// \brief Describes information about particular language setup that can be used in one or more locales.
struct kan_resource_language_t
{
    kan_interned_string_t unicode_language_id;
    kan_interned_string_t unicode_script_id;
    enum kan_language_horizontal_direction_t horizontal_direction;
    enum kan_language_vertical_direction_t vertical_direction;
};

RESOURCE_LOCALE_API void kan_resource_language_init (struct kan_resource_language_t *instance);

KAN_C_HEADER_END
