#pragma once

#include <text_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/interned_string.h>
#include <kan/context/render_backend_system.h>
#include <kan/inline_math/inline_math.h>
#include <kan/reflection/markup.h>

// TODO: Docs.

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_text_t);
KAN_HANDLE_DEFINE (kan_font_library_t);

typedef uint32_t kan_unicode_codepoint_t;

/// \brief Returns next unicode codepoint or 0 when iteration ended or text is malformed.
/// \param iterator Pointer to pointer that is used as string iterator and modified by this function.
TEXT_API kan_unicode_codepoint_t kan_text_utf8_next (const uint8_t **iterator);

enum kan_text_orientation_t
{
    KAN_TEXT_ORIENTATION_HORIZONTAL = 0u,
    KAN_TEXT_ORIENTATION_VERTICAL,
};

enum kan_text_reading_direction_t
{
    KAN_TEXT_READING_DIRECTION_LEFT_TO_RIGHT = 0u,
    KAN_TEXT_READING_DIRECTION_RIGHT_TO_LEFT,
};

enum kan_text_item_type_t
{
    /// \brief Used for simplicity when empty parameters are skipped during formatting, for example.
    KAN_TEXT_ITEM_EMPTY = 0u,

    /// \brief Used to append utf8 text slice as null terminated string.
    KAN_TEXT_ITEM_UTF8,

    /// \brief Used to insert glyph-sized icon into the text represented by icon name.
    KAN_TEXT_ITEM_ICON,

    /// \brief Used to push styles and mark indices.
    KAN_TEXT_ITEM_STYLE,
};

struct kan_text_item_icon_t
{
    uint32_t icon_index;

    /// \brief Codepoint used to calculate proper extents for the icon. Special characters like 0x25A0 are advised.
    uint32_t base_codepoint;

    float x_scale;
    float y_scale;
};

struct kan_text_item_style_t
{
    kan_interned_string_t style;
    uint32_t mark_index;
};

struct kan_text_item_t
{
    enum kan_text_item_type_t type;
    union
    {
        /// \details We do not perform full scale bidi for this fragments. Instead, neutral characters are assigned to
        ///          the script of the previous non-neutral character or to the next non-neutral character if there is
        ///          no previous one. It can result in artifacts, therefore we use '\u0091' character as "bidi reset"
        ///          character which means that characters after "bidi reset" character ignore characters prior to
        ///          "bidi reset" character when selecting script for neutral characters. '\u0091' character itself is
        ///          not added to the built text. '\u0091' is marked as private use character in unicode, so it should
        ///          be fine to use it like that.
        const char *utf8;

        struct kan_text_item_icon_t icon;
        struct kan_text_item_style_t style;
    };
};

TEXT_API kan_text_t kan_text_create (kan_instance_size_t items_count, struct kan_text_item_t *items);

TEXT_API void kan_text_destroy (kan_text_t instance);

struct kan_text_shaped_glyph_instance_data_t
{
    struct kan_float_vector_2_t min;
    struct kan_float_vector_2_t max;
    struct kan_float_vector_2_t uv_min;
    struct kan_float_vector_2_t uv_max;
    uint32_t layer;
    uint32_t mark_index;
};

struct kan_text_shaped_icon_instance_data_t
{
    struct kan_float_vector_4_t min;
    struct kan_float_vector_4_t max;
    uint32_t icon_index;
    uint32_t mark_index;
};

struct kan_text_shaped_data_t
{
    struct kan_int32_vector_2_t min;
    struct kan_int32_vector_2_t max;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_text_shaped_glyph_instance_data_t)
    struct kan_dynamic_array_t glyphs;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_text_shaped_icon_instance_data_t)
    struct kan_dynamic_array_t icons;
};

TEXT_API void kan_text_shaped_data_init (struct kan_text_shaped_data_t *instance);

TEXT_API void kan_text_shaped_data_shutdown (struct kan_text_shaped_data_t *instance);

struct kan_font_library_category_t
{
    /// \brief Unicode script id in iso15924 format.
    kan_interned_string_t script;

    kan_interned_string_t style;

    kan_instance_size_t variable_axis_count;
    float *variable_axis;

    kan_memory_size_t data_size;
    void *data;
};

TEXT_API kan_font_library_t kan_font_library_create (kan_render_context_t render_context,
                                                     kan_instance_size_t categories_count,
                                                     struct kan_font_library_category_t *categories);

TEXT_API kan_render_image_t kan_font_library_get_sdf_atlas (kan_font_library_t instance);

enum kan_font_glyph_render_format_t
{
    KAN_FONT_GLYPH_RENDER_FORMAT_SDF = 0u,
};

enum kan_text_shaping_alignment_t
{
    /// \details For vertical orientation, treated as top alignment.
    KAN_TEXT_SHAPING_ALIGNMENT_LEFT = 0u,

    KAN_TEXT_SHAPING_ALIGNMENT_CENTER,

    /// \details For vertical orientation, treated as bottom alignment.
    KAN_TEXT_SHAPING_ALIGNMENT_RIGHT,
};

struct kan_text_shaping_request_t
{
    uint32_t font_size;
    enum kan_font_glyph_render_format_t render_format;
    enum kan_text_orientation_t orientation;
    enum kan_text_reading_direction_t reading_direction;
    enum kan_text_shaping_alignment_t alignment;
    kan_instance_size_t primary_axis_limit;
    kan_text_t text;
};

TEXT_API bool kan_font_library_shape (kan_font_library_t instance,
                                      struct kan_text_shaping_request_t *request,
                                      struct kan_text_shaped_data_t *output);

struct kan_text_precache_request_t
{
    kan_interned_string_t script;
    kan_interned_string_t style;
    enum kan_font_glyph_render_format_t render_format;
    enum kan_text_orientation_t orientation;
    const char *utf8;
};

/// \brief Precaches glyph data on atlas for nominal glyphs specified by codepoints from given utf8 string.
TEXT_API bool kan_font_library_precache (kan_font_library_t instance, struct kan_text_precache_request_t *request);

TEXT_API void kan_font_library_destroy (kan_font_library_t instance);

KAN_C_HEADER_END
