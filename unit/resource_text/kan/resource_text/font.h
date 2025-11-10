#pragma once

#include <resource_text_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/reflection/markup.h>

/// \file
/// \brief Declares font library resource which is used to work with fonts from the application.
///
/// \par Font library
/// \parblock
/// Font library declares set of font faces and their configurations for usage in application. Usually, you don't need
/// more than one font library for the application, but there are corner cases like UI editor app, where editor font
/// library would be needed for editor controls and game font library would be needed to preview the actual edited UI.
/// In that case, `kan_resource_font_library_t::usage_class` should be used. Also, it is advised to leave game usage
/// class at default NULL value.
///
/// Font library consists of categories, where category is a set of font faces used for particular unicode script,
/// for example latin or cyrillic. Each category contains an array of styles -- named font faces. Default style should
/// have NULL name. Style points to the actual font face data as ttf or otf file and also adds values for variable
/// font axes if variable font is used. Also, every category has array of characters to be precached for rendering for
/// horizontal and for vertical orientations. Precaching is done for every style in category.
///
/// When loading font library, categories are chosen based on current selected locale by matching
/// `kan_resource_font_style_t::used_for_languages` with `kan_resource_locale_t::font_languages`. Only categories that
/// are used for enabled languages are loaded at runtime. Also, font libraries are root resources and are always loaded
/// when registered.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Describes one font style of some category.
struct kan_resource_font_style_t
{
    /// \brief Name of this style or `NULL` for the default style.
    kan_interned_string_t style;

    /// \brief Font data file name, preferably in OpenType format.
    /// \details We need to use real font file in runtime due to several reasons. First of all, OpenType fonts support
    ///          advanced kerning for better typography. Also, some languages have enormous amounts of glyphs and it
    ///          would be impossible to precache them all in required sizes to atlases effectively.
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

/// \brief Describes particular category of the font library.
struct kan_resource_font_category_t
{
    /// \brief Script for which this category should be used in iso 15924 format.
    kan_interned_string_t script;

    /// \brief Lists languages for which this category can be used when script matches.
    /// \invariant Must have at least one value, otherwise it will never be loaded. This field is only needed for
    ///            filtering categories to load for current locale.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t used_for_languages;

    /// \brief Sequence of characters in utf8 formats, nominal glyphs for which
    ///        will be precached for horizontal orientation.
    char *precache_utf8_horizontal;

    /// \brief Sequence of characters in utf8 formats, nominal glyphs for which
    ///        will be precached for vertical orientation.
    char *precache_utf8_vertical;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_font_style_t)
    struct kan_dynamic_array_t styles;
};

RESOURCE_TEXT_API void kan_resource_font_category_init (struct kan_resource_font_category_t *instance);

RESOURCE_TEXT_API void kan_resource_font_category_shutdown (struct kan_resource_font_category_t *instance);

/// \brief Describes one font library resource.
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
