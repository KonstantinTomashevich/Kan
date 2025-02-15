#pragma once

#include <universe_render_foundation_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/context/render_backend_system.h>
#include <kan/resource_material/resource_material_instance.h>
#include <kan/universe/universe.h>
#include <kan/universe_object/universe_object.h>
#include <kan/universe_render_foundation/texture.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

KAN_C_HEADER_BEGIN

/// \brief Group that is used to add all render foundation material instance management mutators.
#define KAN_RENDER_FOUNDATION_MATERIAL_INSTANCE_MANAGEMENT_MUTATOR_GROUP                                               \
    "render_foundation_material_instance_management"

/// \brief Checkpoint, after which render foundation material instance management mutators are executed.
#define KAN_RENDER_FOUNDATION_MATERIAL_INSTANCE_MANAGEMENT_BEGIN_CHECKPOINT                                            \
    "render_foundation_material_instance_management_begin"

/// \brief Checkpoint, that is hit after all render foundation material instance management mutators finished execution.
#define KAN_RENDER_FOUNDATION_MATERIAL_INSTANCE_MANAGEMENT_END_CHECKPOINT                                              \
    "render_foundation_material_instance_management_end"

KAN_TYPED_ID_32_DEFINE (kan_render_material_instance_usage_id_t);

/// \brief Used to inform material instance management that material instance needs to be loaded.
struct kan_render_material_instance_usage_t
{
    /// \brief This usage unique id, must be generated from `kan_next_material_instance_usage_id`.
    kan_render_material_instance_usage_id_t usage_id;

    /// \brief Name of the material instance asset to be loaded.
    kan_interned_string_t name;

    /// \brief Index of the best mip that is advised to be loaded for referenced textures.
    /// \details See `kan_render_texture_usage_t`.
    uint8_t image_best_advised_mip;

    /// \brief Index of the worst mip that is advised to be loaded for referenced textures.
    /// \details See `kan_render_texture_usage_t`.
    uint8_t image_worst_advised_mip;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_usage_init (
    struct kan_render_material_instance_usage_t *instance);

// TODO: We do not use cascade deletion here as we would still need to cleanup instantiation data.

struct kan_render_material_instance_custom_instanced_parameter_t
{
    kan_render_material_instance_usage_id_t usage_id;
    struct kan_resource_material_parameter_t parameter;
};

// TODO: Perhaps, it is logical to leave only custom instanced parameters for now?
//       It is kind of reasonable not to edit the buffers in custom instances.
//       Of course, other engines do that. But we might get away with tails and instanced parameters.
//       And it might be more effective even.

struct kan_render_material_instance_custom_parameter_t
{
    kan_render_material_instance_usage_id_t usage_id;
    struct kan_resource_material_parameter_t parameter;
};

struct kan_render_material_instance_custom_image_t
{
    // TODO: Do we need to support custom images right now?
    //       And are we sure that this should be the texture name, not real image object? What about runtime images?
    kan_render_material_instance_usage_id_t usage_id;
    struct kan_resource_material_image_t image;
    kan_render_texture_usage_id_t allocated_texture_usage_id;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_custom_image_init (
    struct kan_render_material_instance_custom_image_t *instance);

/// \brief Singleton for material instance management, primary used to assign material instance usage ids.
struct kan_render_material_instance_singleton_t
{
    KAN_REFLECTION_IGNORE
    struct kan_atomic_int_t usage_id_counter;

    /// \brief Stub is needed so singleton has at least one field.
    kan_instance_size_t stub_field;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_singleton_init (
    struct kan_render_material_instance_singleton_t *instance);

/// \brief Inline helper for generation of material usage ids.
static inline kan_render_material_instance_usage_id_t kan_next_material_instance_usage_id (
    const struct kan_render_material_instance_singleton_t *singleton)
{
    // Intentionally request const and de-const it to show that it is multithreading-safe function.
    return KAN_TYPED_ID_32_SET (
        kan_render_material_instance_usage_id_t,
        (kan_id_32_t) kan_atomic_int_add ((struct kan_atomic_int_t *) &singleton->usage_id_counter, 1));
}

/// \brief Alignment used for buffer data in `kan_render_material_instance_loaded_data_t::combined_instanced_data`.
#define KAN_RENDER_MATERIAL_INSTANCE_INLINED_INSTANCED_DATA_ALIGNMENT _Alignof (struct kan_float_matrix_4x4_t)

struct kan_render_material_instance_loaded_data_t
{
    kan_interned_string_t material_name;

    /// \details Not owned.
    kan_render_pipeline_parameter_set_t parameter_set;

    /// \details Combined instanced data for better cache coherency.
    ///          Data for every instanced attribute buffer goes one after another in the same order as buffers in meta.
    ///          Every buffer start is aligned using KAN_RENDER_MATERIAL_INSTANCE_INLINED_INSTANCED_DATA_ALIGNMENT.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (uint8_t)
    struct kan_dynamic_array_t combined_instanced_data;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_loaded_data_init (
    struct kan_render_material_instance_loaded_data_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_loaded_data_shutdown (
    struct kan_render_material_instance_loaded_data_t *instance);

struct kan_render_material_instance_loaded_t
{
    kan_interned_string_t name;
    struct kan_render_material_instance_loaded_data_t data;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_loaded_init (
    struct kan_render_material_instance_loaded_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_loaded_shutdown (
    struct kan_render_material_instance_loaded_t *instance);

struct kan_render_material_instance_custom_loaded_t
{
    kan_render_material_instance_usage_id_t usage_id;
    struct kan_render_material_instance_loaded_data_t data;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_custom_loaded_init (
    struct kan_render_material_instance_custom_loaded_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_custom_loaded_shutdown (
    struct kan_render_material_instance_custom_loaded_t *instance);

KAN_C_HEADER_END
