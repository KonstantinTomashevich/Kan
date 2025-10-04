#pragma once

#include <universe_render_foundation_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/context/render_backend_system.h>
#include <kan/render_pipeline_language/compiler.h>
#include <kan/threading/atomic.h>
#include <kan/universe/universe.h>

/// \file
/// \brief Provides API for render foundation render pass, material and material instance resource management.
///
/// \par Definition
/// \parblock
/// This file unifies API for interacting with render passes, materials and material instances and their management
/// in runtime. These resources are tightly coupled on render implementation level and therefore need to be managed
/// by unified system that also can unify hot reload routine for these resources to avoid unnecessary complexities.
/// It was decided to call that unified routine "program management" as combination of passes, materials and material
/// instance defines something that can be called render programs and their data.
/// \endparblock
///
/// \par Render passes
/// \parblock
/// All found render pass resources are loaded and prepared for usage automatically, because in most cases all render
/// passes are required for the application, therefore there is no sense to load them on demand.
/// \endparblock
///
/// \par Materials
/// \parblock
/// All found material resources are also loaded and prepared for usage automatically, however materials that are
/// already referenced by material instances have higher loading and pipeline compilation priority. The reason for that
/// is the fact that it is generally considered a good practice to prepare all GPU pipelines for use as soon as
/// possible, ideally during game startup and main menu phases as pipeline build could be relatively costly operation.
/// \endparblock
///
/// \par Material instances
/// \parblock
/// Material instances are only loaded when there is at least one `kan_render_material_instance_usage_t` that is
/// referencing this material instance. When there is no usages, material instance is automatically unloaded.
/// Also, usage dictates best and worst required mips for textures required by this material. All variants are loaded
/// for the material instance so users can extract their instance data right away.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Group that is used to add all render foundation program management mutators.
#define KAN_RENDER_FOUNDATION_PROGRAM_MANAGEMENT_MUTATOR_GROUP "render_foundation_program_management"

/// \brief Checkpoint, after which render foundation program management mutators are executed.
#define KAN_RENDER_FOUNDATION_PROGRAM_MANAGEMENT_BEGIN_CHECKPOINT "render_foundation_program_management_begin"

/// \brief Checkpoint, that is hit after all render foundation program management mutators have finished execution.
#define KAN_RENDER_FOUNDATION_PROGRAM_MANAGEMENT_END_CHECKPOINT "render_foundation_program_management_end"

KAN_TYPED_ID_32_DEFINE (kan_render_material_instance_usage_id_t);

/// \brief Singleton for publicly accessible data related to program management.
struct kan_render_program_singleton_t
{
    /// \brief Internal counter for generating material usage ids.
    struct kan_atomic_int_t material_instance_usage_id_counter;

    /// \brief Count of passes that are currently being loaded.
    kan_instance_size_t pass_loading_counter;

    /// \brief Count of materials that are currently being loaded.
    /// \details Material becomes loaded when its pipelines are created and scheduled for compilation.
    ///          It means that material can be technically loaded while pipelines are not yet built and render backend
    ///          needs to do some more work to build them.
    kan_instance_size_t material_loading_counter;

    /// \brief Count of material instances that are currently being loaded.
    kan_instance_size_t material_instance_loading_counter;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_program_singleton_init (struct kan_render_program_singleton_t *instance);

/// \brief Inline helper for generation of material usage ids.
static inline kan_render_material_instance_usage_id_t kan_next_material_instance_usage_id (
    const struct kan_render_program_singleton_t *singleton)
{
    // Intentionally request const and de-const it to show that it is multithreading-safe function.
    return KAN_TYPED_ID_32_SET (kan_render_material_instance_usage_id_t,
                                (kan_id_32_t) kan_atomic_int_add (
                                    (struct kan_atomic_int_t *) &singleton->material_instance_usage_id_counter, 1));
}

/// \brief Contains layout and binding information about single variant for pipelines inside render pass.
struct kan_render_foundation_pass_variant_t
{
    kan_interned_string_t name;

    /// \details Can be invalid handle when pipeline has empty parameter set layout.
    kan_render_pipeline_parameter_set_layout_t pass_parameter_set_layout;

    /// \brief Bindings meta for pass pipeline parameter set.
    struct kan_rpl_meta_set_bindings_t pass_parameter_set_bindings;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_foundation_pass_variant_init (
    struct kan_render_foundation_pass_variant_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_foundation_pass_variant_shutdown (
    struct kan_render_foundation_pass_variant_t *instance);

/// \brief Stores information about pass attachment that could be useful for outer users.
struct kan_render_foundation_pass_attachment_t
{
    enum kan_render_pass_attachment_type_t type;
    enum kan_render_image_format_t format;
};

/// \brief Represents loaded and successfully created render pass.
struct kan_render_foundation_pass_loaded_t
{
    kan_interned_string_t name;
    enum kan_render_pass_type_t type;
    kan_render_pass_t pass;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_foundation_pass_attachment_t)
    struct kan_dynamic_array_t attachments;

    /// \brief Information about layout and binding for pass pipeline variants in the same order as in resource.
    /// \details Will be empty for passes that do not have any pass customization (no special set layout).
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_foundation_pass_variant_t)
    struct kan_dynamic_array_t variants;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_foundation_pass_loaded_init (
    struct kan_render_foundation_pass_loaded_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_foundation_pass_loaded_shutdown (
    struct kan_render_foundation_pass_loaded_t *instance);

/// \brief Event that is being sent when `kan_render_foundation_pass_t` is updated.
struct kan_render_foundation_pass_updated_event_t
{
    kan_interned_string_t name;
};

/// \brief Stores information about loaded and instance pipeline for particular pass.
struct kan_render_material_pipeline_t
{
    /// \brief Name of the pass for which pipeline was created.
    kan_interned_string_t pass_name;

    /// \brief Index of this pipeline in pass variants.
    kan_interned_string_t variant_name;

    /// \brief Handle to the actual pipeline. Might still be in compilation stage.
    kan_render_graphics_pipeline_t pipeline;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_pipeline_init (struct kan_render_material_pipeline_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_pipeline_shutdown (
    struct kan_render_material_pipeline_t *instance);

///\brief Contains material loaded data: its parameter set layouts, pipelines and family meta.
struct kan_render_material_loaded_t
{
    /// \brief Material resource name.
    kan_interned_string_t name;

    /// \brief Array with currently instanced pipelines for existing passes.
    /// \details It is advised to cache pipeline handles in separate render cache record in the order that is
    ///          convenient for the render implementation.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_material_pipeline_t)
    struct kan_dynamic_array_t pipelines;

    /// \brief Layout for material set of parameters for pipeline.
    kan_render_pipeline_parameter_set_layout_t set_material;

    /// \brief Layout for object set of parameters for pipeline.
    kan_render_pipeline_parameter_set_layout_t set_object;

    /// \brief Layout for shared set of parameters for pipeline.
    kan_render_pipeline_parameter_set_layout_t set_shared;

    /// \brief Information about vertex attribute sources used by this material.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_meta_attribute_source_t)
    struct kan_dynamic_array_t vertex_attribute_sources;

    /// \brief Size of push constant for pipelines of this material or zero if push constants are not used.
    kan_instance_size_t push_constant_size;

    /// \brief Whether this material has instanced attribute source.
    bool has_instanced_attribute_source;

    /// \brief Information about instanced attribute source for this material if it exists.
    KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (has_instanced_attribute_source)
    KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (true)
    struct kan_rpl_meta_attribute_source_t instanced_attribute_source;

    /// \brief Information about bindings for material set of this material.
    struct kan_rpl_meta_set_bindings_t set_material_bindings;

    /// \brief Information about bindings for object set of this material.
    struct kan_rpl_meta_set_bindings_t set_object_bindings;

    /// \brief Information about bindings for unstable set of this material.
    struct kan_rpl_meta_set_bindings_t set_shared_bindings;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_loaded_init (struct kan_render_material_loaded_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_loaded_shutdown (struct kan_render_material_loaded_t *instance);

/// \brief Sent when loaded material is inserted or updated, including pipeline update due to pass-related operations.
struct kan_render_material_updated_event_t
{
    kan_interned_string_t name;
};

/// \brief Used to inform program management that material instance needs to be loaded.
/// \warning Just like low level resource usages, material instance usages are never intended to be changed,
///          only deleted and inserted. The reasons are the same as for resource usages.
struct kan_render_material_instance_usage_t
{
    /// \brief This usage unique id, must be generated from `kan_next_material_instance_usage_id`.
    kan_render_material_instance_usage_id_t usage_id;

    /// \brief Name of the material instance asset to be loaded.
    kan_interned_string_t name;

    /// \brief Index of the best mip that is advised to be loaded for material textures.
    /// \details For example, when there is no usages that advise mip 0, it won't be loaded.
    uint8_t best_advised_mip;

    /// \brief Index of the worst mip that is advised to be loaded for material textures.
    /// \details For example, if we know that mips 2 and 3 are never needed, we can save memory and do not load them.
    uint8_t worst_advised_mip;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_usage_init (
    struct kan_render_material_instance_usage_t *instance);

/// \brief Describes loaded material instance variant.
struct kan_render_material_instance_variant_t
{
    kan_interned_string_t name;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (uint8_t)
    struct kan_dynamic_array_t instanced_data;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_variant_init (
    struct kan_render_material_instance_variant_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_variant_shutdown (
    struct kan_render_material_instance_variant_t *instance);

/// \brief Contains buffer used by material instance, mostly needed for internal purposes.
struct kan_render_material_instance_bound_buffer_t
{
    kan_instance_size_t binding;
    kan_render_buffer_t buffer;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_bound_buffer_init (
    struct kan_render_material_instance_bound_buffer_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_bound_buffer_shutdown (
    struct kan_render_material_instance_bound_buffer_t *instance);

///\brief Contains material instance loaded data: its material parameter set and variants.
struct kan_render_material_instance_loaded_t
{
    kan_interned_string_t name;
    kan_interned_string_t material_name;

    /// \brief Built parameter set for material set with data from this material instance.
    kan_render_pipeline_parameter_set_t parameter_set;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_material_instance_variant_t)
    struct kan_dynamic_array_t variants;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_material_instance_bound_buffer_t)
    struct kan_dynamic_array_t bound_buffers;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_loaded_init (
    struct kan_render_material_instance_loaded_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_material_instance_loaded_shutdown (
    struct kan_render_material_instance_loaded_t *instance);

KAN_C_HEADER_END
