#pragma once

#include <universe_render_foundation_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/context/render_backend_system.h>
#include <kan/universe/universe.h>
#include <kan/universe_object/universe_object.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

// TODO: Docs.

KAN_C_HEADER_BEGIN

/// \brief Group that is used to add all render foundation material management mutators.
#define KAN_RENDER_FOUNDATION_MATERIAL_MANAGEMENT_MUTATOR_GROUP "render_foundation_material_management"

/// \brief Checkpoint, after which render foundation material management mutators are executed.
#define KAN_RENDER_FOUNDATION_MATERIAL_MANAGEMENT_BEGIN_CHECKPOINT "render_foundation_material_management_begin"

/// \brief Checkpoint, that is hit after all render foundation material management mutators finished execution.
#define KAN_RENDER_FOUNDATION_MATERIAL_MANAGEMENT_END_CHECKPOINT "render_foundation_material_management_end"

KAN_TYPED_ID_32_DEFINE (kan_render_material_usage_id_t);

/// \brief Used to inform material management that material needs to be loaded.
/// \details Pipelines are instanced automatically based on which render passes are available.
struct kan_render_material_usage_t
{
    /// \brief This usage unique id, must be generated from `kan_next_material_usage_id`.
    kan_render_material_usage_id_t usage_id;

    /// \brief Name of the material asset to be loaded.
    kan_interned_string_t name;
};

/// \brief Singleton for material management, primary used to assign material usage ids.
struct kan_render_material_singleton_t
{
    KAN_REFLECTION_IGNORE
    struct kan_atomic_int_t usage_id_counter;

    /// \brief Stub is needed so singleton has at least one field.
    kan_instance_size_t stub_field;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_singleton_init (
    struct kan_render_material_singleton_t *instance);

/// \brief Inline helper for generation of material usage ids.
static inline kan_render_material_usage_id_t kan_next_material_usage_id (
    const struct kan_render_material_singleton_t *material_singleton)
{
    // Intentionally request const and de-const it to show that it is multithreading-safe function.
    return KAN_TYPED_ID_32_SET (
        kan_render_material_usage_id_t,
        (kan_id_32_t) kan_atomic_int_add ((struct kan_atomic_int_t *) &material_singleton->usage_id_counter, 1));
}

struct kan_render_material_loaded_pipeline_t
{
    kan_interned_string_t pass_name;

    /// \details Not owned, just copied handle.
    kan_render_graphics_pipeline_t pipeline;
};

struct kan_render_material_loaded_t
{
    kan_interned_string_t name;
    kan_render_graphics_pipeline_family_t family;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_material_loaded_pipeline_t)
    struct kan_dynamic_array_t pipelines;

    struct kan_rpl_meta_t family_meta;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_loaded_init (struct kan_render_material_loaded_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_loaded_shutdown (struct kan_render_material_loaded_t *instance);

/// \details Sent when material itself is updated, not its pipelines.
struct kan_render_material_updated_event_t
{
    kan_interned_string_t name;
};

KAN_C_HEADER_END
