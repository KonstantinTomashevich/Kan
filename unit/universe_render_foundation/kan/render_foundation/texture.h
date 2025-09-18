#pragma once

#include <universe_render_foundation_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/context/render_backend_system.h>
#include <kan/universe/universe.h>
#include <kan/universe_object/universe_object.h>

/// \file
/// \brief Provides API for interacting with render foundation texture management implementation.
///
/// \par Definition
/// \parblock
/// Render foundation texture management automatically loads and unloads textures based on `kan_render_texture_usage_t`
/// instances. Render foundation checks all usages and loads only the mip intervals that are advised by at least one
/// usage. When texture is loaded, `kan_render_texture_loaded_t` instance is created with appropriate render image.
/// When there is no more usages, `kan_render_texture_loaded_t` is automatically deleted. When mip requirements change
/// or texture data changes (due to hot reload, for example), `kan_render_texture_loaded_t` is automatically updated.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Group that is used to add all render foundation texture management mutators.
#define KAN_RENDER_FOUNDATION_TEXTURE_MANAGEMENT_MUTATOR_GROUP "render_foundation_texture_management"

/// \brief Checkpoint, after which render foundation texture management mutators are executed.
#define KAN_RENDER_FOUNDATION_TEXTURE_MANAGEMENT_BEGIN_CHECKPOINT "render_foundation_texture_management_begin"

/// \brief Checkpoint, that is hit after all render foundation texture management mutators have finished execution.
#define KAN_RENDER_FOUNDATION_TEXTURE_MANAGEMENT_END_CHECKPOINT "render_foundation_texture_management_end"

KAN_TYPED_ID_32_DEFINE (kan_render_texture_usage_id_t);

/// \brief Used to inform texture management that texture needs to be loaded and given mips need to be present.
/// \details When there is not enough memory, mips are allowed to be unloaded to save memory, therefore fields have
///          "advised" in their names. Also, different usages might require different mips and loaded texture will
///          contain all of them.
/// \warning Just like low level resource usages, texture usages are never intended to be changed, only deleted and
///          inserted. The reasons are the same as for resource usages.
struct kan_render_texture_usage_t
{
    /// \brief This usage unique id, must be generated from `kan_next_texture_usage_id`.
    kan_render_texture_usage_id_t usage_id;

    /// \brief Name of the texture asset to be loaded.
    kan_interned_string_t name;

    /// \brief Index of the best mip that is advised to be loaded.
    /// \details For example, when there is no usages that advise mip 0, it won't be loaded.
    uint8_t best_advised_mip;

    /// \brief Index of the worst mip that is advised to be loaded.
    /// \details For example, if we know that mips 2 and 3 are never needed, we can save memory and do not load them.
    uint8_t worst_advised_mip;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_texture_usage_init (struct kan_render_texture_usage_t *instance);

/// \brief Singleton for texture management, primary used to assign texture usage ids.
struct kan_render_texture_singleton_t
{
    struct kan_atomic_int_t usage_id_counter;
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

/// \brief Contains render image with data from texture with given name.
/// \details Image mips are not the same as texture mips. For example, if texture has 5 mips and we only need mips
///          1, 2 and 3, image would have 3 mips and image mip 0 will be the texture mip 1 and so on.
struct kan_render_texture_loaded_t
{
    kan_interned_string_t name;
    kan_render_image_t image;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_texture_loaded_shutdown (struct kan_render_texture_loaded_t *instance);

/// \brief Event that is being sent when `kan_render_texture_loaded_t` is inserted or its image is updated.
struct kan_render_texture_updated_event_t
{
    kan_interned_string_t name;
};

KAN_C_HEADER_END
