#pragma once

#include <resource_text_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/reflection/markup.h>

// TODO: Docs.

KAN_C_HEADER_BEGIN

struct kan_resource_font_style_t
{
    /// \brief Styles are used for additional visual selection step after language-based selection,
    ///        for example to select bold or italic font face.
    kan_interned_string_t style;

    /// \brief Font data file name, preferably in OpenType format.
    /// \details We need to use real font file in runtime due to several reasons. First of all, OpenType fonts support
    ///          advanced kerning for better typography. Also, some languages have enormous amounts of glyphs and it
    ///          would be impossible to prerender them all in required sizes to atlases effectively.
    /// \invariant When several styles or categories select the same font file, even if these categories belong to
    ///            different font libraries, font file is guaranteed to be loaded in runtime only once, which is very
    ///            important when using variable fonts.
    kan_interned_string_t font_data_file;

    /// \brief If using variable font, stores values for variable axes.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (float)
    struct kan_dynamic_array_t variable_font_axes;
};

RESOURCE_TEXT_API void kan_resource_font_style_init (struct kan_resource_font_style_t *instance);

RESOURCE_TEXT_API void kan_resource_font_style_shutdown (struct kan_resource_font_style_t *instance);

struct kan_resource_font_category_t
{
    /// \brief Script for which this font should be used in iso 15924 format.
    kan_interned_string_t script;

    /// \brief Lists languages for which this category can be used.
    /// \invariant Must have at least one value, otherwise it will never be loaded. This field is only needed for
    ///            filtering categories to load for current locale.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t used_for_languages;

    char *precache_utf8;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_font_style_t)
    struct kan_dynamic_array_t styles;
};

RESOURCE_TEXT_API void kan_resource_font_category_init (struct kan_resource_font_category_t *instance);

RESOURCE_TEXT_API void kan_resource_font_category_shutdown (struct kan_resource_font_category_t *instance);

struct kan_resource_font_library_t
{
    /// \brief Usage class name for applications that may use several font libraries.
    /// \details Some applications, for example something like UI editor, might need several libraries -- application
    ///          specific library for application text and game library for game text. For that cases, specific
    ///          libraries should specify their usage class. Game library should leave this field empty.
    kan_interned_string_t usage_class;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_font_category_t)
    struct kan_dynamic_array_t categories;
};

RESOURCE_TEXT_API void kan_resource_font_library_init (struct kan_resource_font_library_t *instance);

RESOURCE_TEXT_API void kan_resource_font_library_shutdown (struct kan_resource_font_library_t *instance);

KAN_C_HEADER_END
