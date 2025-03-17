#pragma once

#include <universe_render_foundation_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/context/render_backend_system.h>
#include <kan/render_pipeline_language/compiler.h>
#include <kan/universe/universe.h>
#include <kan/universe_object/universe_object.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

/// \file
/// \brief Provides API for interacting with render foundation material management implementation.
///
/// \par Definition
/// \parblock
/// Render foundation material management automatically loads and unloads materials based on
/// `kan_render_material_usage_t` instances. Pipelines are automatically loaded and scheduled for compilation based
/// on loaded render pass presence. Pipelines can be deleted from loaded material and/or inserted back as a response
/// to render pass reload, deletion or insertion. Material hot reload including code changes is supported.
///
/// Also, material management requires configuration with name KAN_RENDER_FOUNDATION_MATERIAL_MANAGEMENT_CONFIGURATION
/// of type `kan_render_material_configuration_t` to be present in its world.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Group that is used to add all render foundation material management mutators.
#define KAN_RENDER_FOUNDATION_MATERIAL_MANAGEMENT_MUTATOR_GROUP "render_foundation_material_management"

/// \brief Checkpoint, after which render foundation material management mutators are executed.
#define KAN_RENDER_FOUNDATION_MATERIAL_MANAGEMENT_BEGIN_CHECKPOINT "render_foundation_material_management_begin"

/// \brief Checkpoint, that is hit after all render foundation material management mutators finished execution.
#define KAN_RENDER_FOUNDATION_MATERIAL_MANAGEMENT_END_CHECKPOINT "render_foundation_material_management_end"

/// \brief Name for render foundation material management configuration object in universe world.
#define KAN_RENDER_FOUNDATION_MATERIAL_MANAGEMENT_CONFIGURATION "render_foundation_material_management"

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

/// \brief Configuration data type for material management routine.
struct kan_render_material_configuration_t
{
    /// \brief Attempts to load all available materials as soon as they're detected.
    /// \details Currently, we have no other solution than to load pre-load materials with the same priority as active
    ///          ones. We can only build their pipelines with lower priorities.
    ///          The reason for that is the fact that there is no proper technique for modifying request priority
    ///          without reawakening sleeping requests or causing reload. We cannot simply change the priority as
    ///          high-level resource management mutators cannot edit requests (as it would result in access mask clash).
    ///          TODO: In the future, we should improve resource provider and solve this issue as well.
    kan_bool_t preload_materials;
};

/// \brief Stores information about loaded and instance pipeline for particular pass.
struct kan_render_material_loaded_pipeline_t
{
    /// \brief Name of the pass for which pipeline was created.
    kan_interned_string_t pass_name;

    /// \brief Index of this pipeline in pass variants.
    kan_instance_size_t variant_index;

    /// \brief Handle to the actual pipeline. Might still be in compilation stage.
    /// \details Not owned, just copied handle.
    kan_render_graphics_pipeline_t pipeline;
};

///\brief Contains material loaded data: its parameter set layouts, pipelines and family meta.
struct kan_render_material_loaded_t
{
    /// \brief Material resource name.
    kan_interned_string_t name;

    /// \brief Layout for material set of parameters for pipeline.
    /// \details Not owned, just copied handle. Can be invalid if set layout is empty.
    kan_render_pipeline_parameter_set_layout_t set_material;

    /// \brief Layout for object set of parameters for pipeline.
    /// \details Not owned, just copied handle. Can be invalid if set layout is empty.
    kan_render_pipeline_parameter_set_layout_t set_object;

    /// \brief Layout for unstable set of parameters for pipeline.
    /// \details Not owned, just copied handle. Can be invalid if set layout is empty.
    kan_render_pipeline_parameter_set_layout_t set_unstable;

    /// \brief Array with currently instanced pipelines for existing passes.
    /// \details Guaranteed to be sorted by passes and variant indices. It means that pipelines are clustered by passes
    ///          and inside every cluster pipelines are guaranteed to be sorted by variant index.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_material_loaded_pipeline_t)
    struct kan_dynamic_array_t pipelines;

    /// \brief Meta of material pipeline family. Does not contains any info about any particular pass.
    struct kan_rpl_meta_t family_meta;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_loaded_init (struct kan_render_material_loaded_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_loaded_shutdown (struct kan_render_material_loaded_t *instance);

/// \brief Sent when loaded material is inserted or updated, including pipeline update due to pass-related operations.
struct kan_render_material_updated_event_t
{
    kan_interned_string_t name;
};

KAN_C_HEADER_END
