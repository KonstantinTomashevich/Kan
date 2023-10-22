#pragma once

#include <reflection_api.h>

#include <kan/api_common/bool.h>
#include <kan/memory_profiler/allocation_group.h>
#include <kan/reflection/registry.h>

/// \file
/// \brief Provides interface for data block patching.
///
/// \par Patching
/// \parblock
/// In this context, patching is modification that is applied to single block of data, usually an instance of some
/// structure, by overriding data at particular offsets. It is useful to define things like prefabs, both in runtime
/// and during serialization. Also, patches are attached to reflection registry and are automatically destroyed when
/// registry is destroyed.
/// \endparblock
///
/// \par Patch building
/// \parblock
/// Patches should be built using `kan_reflection_patch_builder_t` instance and `kan_reflection_patch_builder_build`.
/// This function ensures that resulting patch is optimized as much as possible and can be applied to required structure
/// (if compiled with KAN_REFLECTION_VALIDATION_ENABLED).
/// \endparblock
///
/// \par Patch invariants
/// \parblock
/// Patches should not override fields of `KAN_REFLECTION_ARCHETYPE_STRING_POINTER`,
/// `KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER`, `KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY`,
/// `KAN_REFLECTION_ARCHETYPE_PATCH` as these archetypes technically pointer to the outer block of memory.
/// \endparblock

KAN_C_HEADER_BEGIN

typedef uint64_t kan_reflection_patch_builder_t;

typedef uint64_t kan_reflection_patch_t;

#define KAN_REFLECTION_INVALID_PATCH 0u

/// \brief Contains information about single patch chunk.
struct kan_reflection_patch_chunk_info_t
{
    uint64_t offset;
    uint64_t size;
    void *data;
};

typedef uint64_t kan_reflection_patch_iterator_t;

/// \brief Creates new instance of patch builder.
REFLECTION_API kan_reflection_patch_builder_t kan_reflection_patch_builder_create ();

/// \brief Adds new chunk to currently constructed patch. Data is copied inside.
REFLECTION_API void kan_reflection_patch_builder_add_chunk (kan_reflection_patch_builder_t builder,
                                                            uint64_t offset,
                                                            uint64_t size,
                                                            void *data);

/// \brief Attempts to build a patch from added chunks and registers it with given type in given registry.
/// \details On completion, removes all added chunks and returns to initial state.
REFLECTION_API kan_reflection_patch_t kan_reflection_patch_builder_build (kan_reflection_patch_builder_t builder,
                                                                          kan_reflection_registry_t registry,
                                                                          const struct kan_reflection_struct_t *type);

/// \brief Destroys given patch builder and frees its resources.
REFLECTION_API void kan_reflection_patch_builder_destroy (kan_reflection_patch_builder_t builder);

/// \brief Gets type of structure for given patch.
REFLECTION_API const struct kan_reflection_struct_t *kan_reflection_patch_get_type (kan_reflection_patch_t patch);

/// \brief Applies given patch to given memory block.
REFLECTION_API void kan_reflection_patch_apply (kan_reflection_patch_t patch, void *target);

/// \brief Returns iterator that points to the first chunk of built patch.
/// \details Keep in mind that during build process chunks can be optimized out and compressed.
REFLECTION_API kan_reflection_patch_iterator_t kan_reflection_patch_begin (kan_reflection_patch_t patch);

/// \brief Returns iterator that points to the memory after the last chunk of built patch.
/// \details Keep in mind that during build process chunks can be optimized out and compressed.
REFLECTION_API kan_reflection_patch_iterator_t kan_reflection_patch_end (kan_reflection_patch_t patch);

/// \brief Moves patch chunk iterator to the next patch.
REFLECTION_API kan_reflection_patch_iterator_t
kan_reflection_patch_iterator_next (kan_reflection_patch_iterator_t iterator);

/// \brief Returns info about chunk to which iterator is pointing right now.
REFLECTION_API struct kan_reflection_patch_chunk_info_t kan_reflection_patch_iterator_get (
    kan_reflection_patch_iterator_t iterator);

/// \brief Destroys given patch.
REFLECTION_API void kan_reflection_patch_destroy (kan_reflection_patch_t patch);

// TODO: Patch migration.

KAN_C_HEADER_END
