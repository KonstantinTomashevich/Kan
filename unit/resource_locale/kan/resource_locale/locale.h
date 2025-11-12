#pragma once

#include <resource_locale_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/error/critical.h>
#include <kan/reflection/markup.h>

/// \file
/// \brief This file stores representation of locale configuration resources.

KAN_C_HEADER_BEGIN

/// \brief Enumerates possible preferred text reading directions for the locale.
/// \details Preferred direction is used as general text direction for readable line breaking and other related tasks.
///          Text script direction is not affected by preferred direction as every unicode script has its own direction.
enum kan_locale_preferred_text_direction_t
{
    KAN_LOCALE_PREFERRED_TEXT_DIRECTION_LEFT_TO_RIGHT = 0u,
    KAN_LOCALE_PREFERRED_TEXT_DIRECTION_RIGHT_TO_LEFT,
};

/// \brief Describes information about particular locale for the application.
struct kan_resource_locale_t
{
    enum kan_locale_preferred_text_direction_t preferred_direction;

    /// \brief Font library categories with that languages will be loaded.
    /// \details Order is important if text has sequences that contain only neutral characters, as these sequences
    ///          will fall back to the first available category. Sticking to iso 639 is advised, but not required.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t font_languages;
};

RESOURCE_LOCALE_API void kan_resource_locale_init (struct kan_resource_locale_t *instance);

RESOURCE_LOCALE_API void kan_resource_locale_shutdown (struct kan_resource_locale_t *instance);

KAN_C_HEADER_END
