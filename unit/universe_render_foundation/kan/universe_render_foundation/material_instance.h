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

/// \file
/// \brief Provides API for interacting with render foundation material instance management implementation.
///
/// \par Definition
/// \parblock
/// Render foundation material instance management automatically loads and unloads material instances based on
/// `kan_render_material_instance_usage_t` instances. Usages control suggested mips for textures bound to material
/// instances through best/worst advised mip fields, which are merged and passed to appropriate texture usages.
/// Buffers and parameter sets are allocated once per material static data, therefore material instances with
/// instanced parameters only are lightweight and easy to use for instancing.
///
/// Custom parameters feature for material instance usages allows to override instanced parameters on per usage basis.
/// Only instanced parameters can be changed on per usage basis as changing static parameters would lead to creation
/// of additional buffers and therefore is a questionable practice from performance point of view. When objects
/// really need complex data on per-instance basis, it is advised to use object parameter set and manual upload of
/// data to this set as it would be much more efficient and such cases are rare. When custom parameters are used for
/// material instance usage, `kan_render_material_instance_custom_loaded_t` entry should be queried by usage id
/// instead of querying `kan_render_material_instance_loaded_t` by material instance name.
/// \endparblock

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

/// \brief Group that is used to add all render foundation material instance custom sync mutators.
/// \details Custom sync group should be placed into every leaf world that does rendering,
///          so custom sync data would always be up to date.
#define KAN_RENDER_FOUNDATION_MATERIAL_INSTANCE_CUSTOM_SYNC_MUTATOR_GROUP                                              \
    "render_foundation_material_instance_custom_sync"

/// \brief Checkpoint, after which render foundation material instance custom parameter sync mutators are executed.
#define KAN_RENDER_FOUNDATION_MATERIAL_INSTANCE_CUSTOM_SYNC_BEGIN_CHECKPOINT                                           \
    "render_foundation_material_instance_custom_sync_begin"

/// \brief Checkpoint, that is hit after all render foundation material instance
///        custom parameter sync mutators finished execution.
#define KAN_RENDER_FOUNDATION_MATERIAL_INSTANCE_CUSTOM_SYNC_END_CHECKPOINT                                             \
    "render_foundation_material_instance_custom_sync_end"

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

/// \brief Can be attached to usage by usage id in order to
///        request material instance with customized instanced parameters.
/// \details It is allowed to add multiple custom instanced parameters to one usage.
struct kan_render_material_instance_custom_instanced_parameter_t
{
    kan_render_material_instance_usage_id_t usage_id;
    struct kan_resource_material_parameter_t parameter;
};

/// \brief Singleton for material instance management, primary used to assign material instance usage ids.
struct kan_render_material_instance_singleton_t
{
    struct kan_atomic_int_t usage_id_counter;

    /// \brief Used to mark material instance custom parameter sync so if multiple leaf worlds have sync point in them,
    ///        we would still avoid unnecessary duplicate updates.
    kan_time_size_t custom_sync_inspection_marker_ns;
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

/// \brief Alignment used for attribute data in `kan_render_material_instance_loaded_data_t::instanced_data`.
#define KAN_RENDER_MATERIAL_INSTANCE_ATTRIBUTE_DATA_ALIGNMENT alignof (struct kan_float_matrix_4x4_t)

/// \brief Material instance loaded data storage structured. Used both for usual and customized instances.
struct kan_render_material_instance_loaded_data_t
{
    /// \brief Name of the material that should be used with this instance.
    kan_interned_string_t material_name;

    /// \brief Parameter set for material set slot of the pipeline.
    /// \details Not owned.
    kan_render_pipeline_parameter_set_t parameter_set;

    /// \brief Storage for material instanced data.
    /// \details We have one storage as only one instanced attribute source is allowed.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (uint8_t)
    struct kan_dynamic_array_t instanced_data;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_loaded_data_init (
    struct kan_render_material_instance_loaded_data_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_loaded_data_shutdown (
    struct kan_render_material_instance_loaded_data_t *instance);

/// \brief Contains loaded data for usual, not customized, material instance.
struct kan_render_material_instance_loaded_t
{
    kan_interned_string_t name;
    struct kan_render_material_instance_loaded_data_t data;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_loaded_init (
    struct kan_render_material_instance_loaded_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_loaded_shutdown (
    struct kan_render_material_instance_loaded_t *instance);

/// \brief Contains loaded data for material instance customized for particular usage.
struct kan_render_material_instance_custom_loaded_t
{
    kan_render_material_instance_usage_id_t usage_id;

    /// \brief Internal implementation field to avoid excessive updates.
    kan_time_size_t last_inspection_time_ns;

    struct kan_render_material_instance_loaded_data_t data;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_custom_loaded_init (
    struct kan_render_material_instance_custom_loaded_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_custom_loaded_shutdown (
    struct kan_render_material_instance_custom_loaded_t *instance);

KAN_C_HEADER_END
