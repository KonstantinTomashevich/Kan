#pragma once

#include <universe_text_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/context/render_backend_system.h>
#include <kan/text/text.h>
#include <kan/threading/atomic.h>
#include <kan/universe/universe.h>

// TODO: Docs. Text management for root and text shaping for leaves.

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

/// \brief Contains counter for usage ids and flag that tells whether resource scan is done.
struct kan_text_shaping_singleton_t
{
    /// \brief Atomic counter for assigning shaping unit ids. Safe to be modified from different threads.
    struct kan_atomic_int_t unit_id_counter;
};

UNIVERSE_RESOURCE_PROVIDER_API void kan_text_shaping_singleton_init (struct kan_text_shaping_singleton_t *instance);

/// \brief Inline helper for generation of text shaping units ids.
static inline kan_text_shaping_unit_id_t kan_next_text_shaping_unit_id (
    const struct kan_text_shaping_singleton_t *text_shaping)
{
    // Intentionally uses const and de-const it to show that it is multithreading-safe function.
    return KAN_TYPED_ID_32_SET (
        kan_text_shaping_unit_id_t,
        (kan_id_32_t) kan_atomic_int_add ((struct kan_atomic_int_t *) &text_shaping->unit_id_counter, 1));
}

struct kan_text_shaped_stable_data_t
{
    struct kan_int32_vector_2_t min;
    struct kan_int32_vector_2_t max;

    kan_instance_size_t glyphs_count;
    kan_instance_size_t icons_count;

    kan_render_buffer_t glyphs;
    kan_render_buffer_t icons;
};

struct kan_text_shaping_unit_t
{
    kan_text_shaping_unit_id_t id;
    kan_interned_string_t library_usage_class;
    struct kan_text_shaping_request_t request;

    bool stable;
    bool dirty;
    bool shaped;
    bool shaped_as_stable;

    /// \brief If shaped, library that was used for shaping. Needed for things like getting atlas.
    /// \warning Not owned, therefore might be already destroyed during hot reload.
    ///          Only access this field after text shaping as text shaping takes care of reshaping after hot reload!
    kan_font_library_t shaped_with_library;

    // Fields below are ignored by reflection as they're not always initialized.
    union
    {
        KAN_REFLECTION_IGNORE
        struct kan_text_shaped_stable_data_t shaped_stable;

        KAN_REFLECTION_IGNORE
        struct kan_text_shaped_data_t shaped_unstable;
    };
};

UNIVERSE_TEXT_API void kan_text_shaping_unit_init (struct kan_text_shaping_unit_t *instance);

UNIVERSE_TEXT_API void kan_text_shaping_unit_shutdown (struct kan_text_shaping_unit_t *instance);

KAN_C_HEADER_END
