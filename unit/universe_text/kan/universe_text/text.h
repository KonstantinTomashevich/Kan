#pragma once

#include <universe_text_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/context/render_backend_system.h>
#include <kan/text/text.h>
#include <kan/threading/atomic.h>
#include <kan/universe/universe.h>

/// \file
/// \brief Provides API for interacting with text management and text shaping implementation.
///
/// \par Text management
/// \parblock
/// Text management mutator group manages font libraries and also takes care of updating them when locale changes.
/// Text management mutator group should be added to root world the same way as other resource management mutators.
/// \endparblock
///
/// \par Text shaping
/// \parblock
/// Text shaping mutator group processes `kan_text_shaping_unit_t` records and updates them with shaped data. It should
/// be added to leaf worlds as shaped data is likely to be used for rendering in the same frame after shaping.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Group that is used to add all text resource management mutators.
#define KAN_TEXT_MANAGEMENT_MUTATOR_GROUP "text_management"

/// \brief Checkpoint, after which text resource management mutators are executed.
#define KAN_TEXT_MANAGEMENT_BEGIN_CHECKPOINT "text_management_begin"

/// \brief Checkpoint, that is hit after all text resource management mutators have finished execution.
#define KAN_TEXT_MANAGEMENT_END_CHECKPOINT "text_management_end"

/// \brief Group that is used to add all text resource shaping mutators.
#define KAN_TEXT_SHAPING_MUTATOR_GROUP "text_shaping"

/// \brief Checkpoint, after which text resource shaping mutators are executed.
#define KAN_TEXT_SHAPING_BEGIN_CHECKPOINT "text_shaping_begin"

/// \brief Checkpoint, that is hit after all text resource shaping mutators have finished execution.
#define KAN_TEXT_SHAPING_END_CHECKPOINT "text_shaping_end"

KAN_TYPED_ID_32_DEFINE (kan_text_shaping_unit_id_t);

/// \brief Contains counter for shaping unit ids.
struct kan_text_shaping_singleton_t
{
    /// \brief Atomic counter for assigning shaping unit ids. Safe to be modified from different threads.
    struct kan_atomic_int_t unit_id_counter;
};

UNIVERSE_TEXT_API void kan_text_shaping_singleton_init (struct kan_text_shaping_singleton_t *instance);

/// \brief Inline helper for generation of text shaping units ids.
static inline kan_text_shaping_unit_id_t kan_next_text_shaping_unit_id (
    const struct kan_text_shaping_singleton_t *text_shaping)
{
    // Intentionally uses const and de-const it to show that it is multithreading-safe function.
    return KAN_TYPED_ID_32_SET (
        kan_text_shaping_unit_id_t,
        (kan_id_32_t) kan_atomic_int_add ((struct kan_atomic_int_t *) &text_shaping->unit_id_counter, 1));
}

/// \brief Data for text that was shaped as stable.
/// \details As stable shaping is expected to rarely be reshaped, it is better to upload shaped data to buffers right
///          away, instead of copying shaped data to frame lifetime allocators every frame.
struct kan_text_shaped_stable_data_t
{
    kan_instance_size_t glyphs_count;
    kan_instance_size_t icons_count;

    kan_render_buffer_t glyphs;
    kan_render_buffer_t icons;
};

/// \brief Data for text that was shaped as unstable. Moved directly from `kan_text_shaped_data_t`.
struct kan_text_shaped_unstable_data_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_text_shaped_glyph_instance_data_t)
    struct kan_dynamic_array_t glyphs;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_text_shaped_icon_instance_data_t)
    struct kan_dynamic_array_t icons;
};

/// \brief Piece of text that should be processed by text shaping routine.
/// \details Text shaping request could be processed in two separate modes: stable and unstable. Stable mode is designed
///          for the text that changes rarely or never changes: it is shaped right after creation and only reshaped if
///          `dirty` flag is true. Unstable mode should be used for text that is changed every frame and should be
///          reshaped anyway. Also, text shaped in stable mode is automatically uploaded to GPU buffers, while unstable
///          text uploading should be managed manually by the user, for example through frame lifetime allocator.
struct kan_text_shaping_unit_t
{
    kan_text_shaping_unit_id_t id;

    /// \brief Usage class of font library if application expects several font libraries.
    /// \details Should be left at default NULL value in most cases.
    kan_interned_string_t library_usage_class;

    /// \brief Shaping request. Text inside is owner by the shaping unit.
    /// \warning `reading_direction` field is automatically reset to the appropriate value from locale when shaping is
    ///          started, therefore it doesn't matter which value user sets there.
    struct kan_text_shaping_request_t request;

    /// \brief Whether this text should be shaped as stable text.
    bool stable;

    /// \brief Whether this text should be reshaped during next execution if it is a stable text.
    bool dirty;

    /// \brief Whether this unit has shaped data.
    bool shaped;

    /// \brief If `shaped`, tells whether it was `stable` when it was last shaped.
    bool shaped_as_stable;

    /// \brief If shaped, library that was used for shaping. Needed for things like getting atlas.
    /// \warning Not owned, therefore might be already destroyed during hot reload.
    ///          Only access this field after text shaping as text shaping takes care of reshaping after hot reload!
    kan_font_library_t shaped_with_library;

    struct kan_int32_vector_2_t shaped_min;
    struct kan_int32_vector_2_t shaped_max;

    union
    {
        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (shaped_as_stable)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (true)
        struct kan_text_shaped_stable_data_t shaped_stable;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (shaped_as_stable)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (false)
        struct kan_text_shaped_unstable_data_t shaped_unstable;
    };
};

UNIVERSE_TEXT_API void kan_text_shaping_unit_init (struct kan_text_shaping_unit_t *instance);

UNIVERSE_TEXT_API void kan_text_shaping_unit_shutdown (struct kan_text_shaping_unit_t *instance);

KAN_C_HEADER_END
