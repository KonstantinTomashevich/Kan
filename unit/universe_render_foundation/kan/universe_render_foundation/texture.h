#pragma once

#include <universe_render_foundation_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/context/render_backend_system.h>
#include <kan/universe/universe.h>
#include <kan/universe_object/universe_object.h>

// TODO: Docs.

KAN_C_HEADER_BEGIN

/// \brief Group that is used to add all render foundation texture management mutators.
#define KAN_RENDER_FOUNDATION_TEXTURE_MANAGEMENT_MUTATOR_GROUP "render_foundation_texture_management"

/// \brief Checkpoint, after which render foundation texture management mutators are executed.
#define KAN_RENDER_FOUNDATION_TEXTURE_MANAGEMENT_BEGIN_CHECKPOINT "render_foundation_texture_management_begin"

/// \brief Checkpoint, that is hit after all render foundation texture management mutators finished execution.
#define KAN_RENDER_FOUNDATION_TEXTURE_MANAGEMENT_END_CHECKPOINT "render_foundation_texture_management_end"

KAN_TYPED_ID_32_DEFINE (kan_render_texture_usage_id_t);

struct kan_render_texture_usage_t
{
    kan_render_texture_usage_id_t usage_id;
    kan_interned_string_t name;
    uint8_t best_advised_mip;
    uint8_t worst_advised_mip;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_texture_usage_init (struct kan_render_texture_usage_t *instance);

struct kan_render_texture_singleton_t
{
    KAN_REFLECTION_IGNORE
    struct kan_atomic_int_t usage_id_counter;

    /// \brief Stab is needed so singleton has at least one field.
    kan_instance_size_t stub_field;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_texture_singleton_init (struct kan_render_texture_singleton_t *instance);

/// \brief Inline helper for generation of texture usage ids.
static inline kan_render_texture_usage_id_t kan_next_texture_usage_id (
    const struct kan_render_texture_singleton_t *texture_singleton)
{
    // Intentionally request const and de-const it to show that it is multithreading-safe function.
    return KAN_TYPED_ID_32_SET (
        kan_render_texture_usage_id_t,
        (kan_id_32_t) kan_atomic_int_add ((struct kan_atomic_int_t *) &texture_singleton->usage_id_counter, 1));
}

struct kan_render_texture_loaded_t
{
    kan_interned_string_t name;
    kan_render_image_t image;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_texture_loaded_shutdown (struct kan_render_texture_loaded_t *instance);

KAN_C_HEADER_END
