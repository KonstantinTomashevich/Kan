#pragma once

#include <reflection_api.h>

#include <kan/api_common/core_types.h>
#include <kan/memory_profiler/allocation_group.h>
#include <kan/reflection/registry.h>

/// \file
/// \brief Provides interface for data patching in memory.
///
/// \par Patching
/// \parblock
/// Patching is a series of modifications that can be applied to main block of memory passed as argument and to
/// other blocks of memory that are visible from the main block, if they are properly declared during patch build
/// process. Patching essentially overrides data using offsets and is not well aware of reflection (which is only
/// used to access child blocks of memory). Section mechanism is used to change data blocks and will be described
/// on building paragraph.
///
/// Patches are very useful for declaring places in other data structures where arbitrary data might be stored,
/// for example component arrays for levels, because patches carry their type and lots of useful data along the way.
/// Also, patches are very useful for things like prefabs as it is technically trivial to implement inheritance
/// mechanism for them due to their layered structure.
///
/// Also, patches are attached to reflection registry and are automatically destroyed when registry is destroyed.
/// \endparblock
///
/// \par Patch building
/// \parblock
/// Patches should be built using `kan_reflection_patch_builder_t` instance and `kan_reflection_patch_builder_build`.
/// This function ensures that resulting patch is optimized as much as possible and can be applied to required structure
/// (if compiled with KAN_REFLECTION_VALIDATION_ENABLED).
///
/// There are two primary functions for building patches:
/// - `kan_reflection_patch_builder_add_section` defines sections -- unique blocks of memory that can be patched.
///   There is always a root section that can be referenced through `KAN_REFLECTION_PATCH_BUILDER_SECTION_ROOT` and
///   collects changes to main memory block. When defining new section, parent section is used to declare from which
///   section pointer to the new section can be acquired. Also, sections can be safely duplicated through this function,
///   because during patch build excessive sections will be removed.
/// - `kan_reflection_patch_builder_add_chunk` passed chunk of data with given offset in section coordinates to
///   given section. During patch build routine chunks will be merged whenever possible.
/// \endparblock
///
/// \par Patch invariants
/// \parblock
/// Patches should not override fields of `KAN_REFLECTION_ARCHETYPE_STRING_POINTER`,
/// `KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER`, `KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY`,
/// `KAN_REFLECTION_ARCHETYPE_PATCH` as these archetypes technically point to the outer block of memory.
/// If patch needs to access data in other block of memory using these fields, sections with appropriate types and
/// offsets should be created.
/// \endparblock
///
/// \par Threading
/// \parblock
/// Patch builder is not thread safe: only one thread can work with one patch builder at a time. However, it is safe
/// to build multiple patches from multiple threads using multiple patch builders for the same registry: registry
/// should check for registration race and process it accordingly. Also, applying patches is fully thread safe,
/// as it does not change patch data.
/// \endparblock

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_reflection_patch_builder_t);
KAN_HANDLE_DEFINE (kan_reflection_patch_builder_section_t);

/// \brief Patch root section that points to the main block of memory.
#define KAN_REFLECTION_PATCH_BUILDER_SECTION_ROOT KAN_HANDLE_SET_INVALID (kan_reflection_patch_builder_section_t)

KAN_HANDLE_DEFINE (kan_reflection_patch_t);
KAN_HANDLE_DEFINE (kan_reflection_patch_iterator_t);
KAN_TYPED_ID_32_DEFINE (kan_reflection_patch_serializable_section_id_t);

/// \brief Defines supported types of patch sections.
/// \details Section type describes how outer section address will be retrieved and how should it be managed.
enum kan_reflection_patch_section_type_t
{
    /// \brief Informs that this section sets data of dynamic array which is stored at section offset.
    /// \details Array is always resized in order to fit the offsets that are passed through chunks. For example,
    ///          if chunks modify 4-th item of the array, it will always be resized to contain at least 4 items.
    ///          When resizing array of structures, structure initializers will be applied if they are found in
    ///          reflection.
    KAN_REFLECTION_PATCH_SECTION_TYPE_DYNAMIC_ARRAY_SET,

    /// \brief Informs that this section appends new item to the dynamic array.
    /// \details Always executed after array sets and therefore always creates new item. Chunk offsets are treated
    ///          as local to the new item offset. Also, if appending several items to the same array, it will be
    ///          resized back to its size after all appends are done, so there will be no dangling unused capacity.
    KAN_REFLECTION_PATCH_SECTION_TYPE_DYNAMIC_ARRAY_APPEND,
};

/// \brief Contains information about single patch chunk with its data.
struct kan_reflection_patch_chunk_info_t
{
    kan_instance_size_t offset;
    kan_instance_size_t size;
    void *data;
};

/// \brief Contains information about single patch section.
/// \details Due to optimization requirements, it is guaranteed that sections are not duplicated, therefore every
///          kan_reflection_patch_section_info_t::section_id is unique in patch scope and there cannot be several
///          nodes for the same section inside a patch.
struct kan_reflection_patch_section_info_t
{
    kan_reflection_patch_serializable_section_id_t parent_section_id;
    kan_reflection_patch_serializable_section_id_t section_id;
    enum kan_reflection_patch_section_type_t type;
    kan_instance_size_t source_offset_in_parent;
};

/// \brief Contains information about single patch node -- section or data chunk.
struct kan_reflection_patch_node_info_t
{
    kan_bool_t is_data_chunk;
    union
    {
        struct kan_reflection_patch_section_info_t section_info;
        struct kan_reflection_patch_chunk_info_t chunk_info;
    };
};

/// \brief Creates new instance of patch builder.
REFLECTION_API kan_reflection_patch_builder_t kan_reflection_patch_builder_create (void);

/// \brief Adds new chunk to currently constructed patch.
/// \details Duplicated sections are later filtered out and their chunks are reassigned to the unique section.
///          Section submission order doesn't matter. The only requirement is that parent sections should be created
///          before child section, which is impossible to break under current API.
REFLECTION_API kan_reflection_patch_builder_section_t
kan_reflection_patch_builder_add_section (kan_reflection_patch_builder_t builder,
                                          kan_reflection_patch_builder_section_t parent_section,
                                          enum kan_reflection_patch_section_type_t section_type,
                                          kan_instance_size_t source_offset_in_parent);

/// \brief Adds new chunk to currently constructed patch within given section.
/// \details Chunk submission order doesn't matter as chunks are optimized during build either way.
///          Data is copied inside.
REFLECTION_API void kan_reflection_patch_builder_add_chunk (kan_reflection_patch_builder_t builder,
                                                            kan_reflection_patch_builder_section_t section,
                                                            kan_instance_size_t offset,
                                                            kan_instance_size_t size,
                                                            const void *data);

/// \brief Attempts to build a patch from added chunks and sections and registers it with given type in given registry.
/// \details On completion, removes all added chunks and returns to initial state.
REFLECTION_API kan_reflection_patch_t kan_reflection_patch_builder_build (kan_reflection_patch_builder_t builder,
                                                                          kan_reflection_registry_t registry,
                                                                          const struct kan_reflection_struct_t *type);

/// \brief Destroys given patch builder and frees its resources.
REFLECTION_API void kan_reflection_patch_builder_destroy (kan_reflection_patch_builder_t builder);

/// \brief Gets type of structure for given patch. Will be NULL if patch was invalidated during registry migration.
REFLECTION_API const struct kan_reflection_struct_t *kan_reflection_patch_get_type (kan_reflection_patch_t patch);

/// \brief Gets count of chunks inside given patch.
REFLECTION_API kan_instance_size_t kan_reflection_patch_get_chunks_count (kan_reflection_patch_t patch);

/// \brief Returns value which is guaranteed to be bigger than all section ids in patch.
REFLECTION_API kan_id_32_t kan_reflection_patch_get_section_id_bound (kan_reflection_patch_t patch);

/// \brief Applies given patch to given memory block and its child blocks through patch sections.
REFLECTION_API void kan_reflection_patch_apply (kan_reflection_patch_t patch, void *target);

/// \brief Returns iterator that points to the first node of built patch.
/// \details Keep in mind that during build process chunks can be optimized out and compressed.
REFLECTION_API kan_reflection_patch_iterator_t kan_reflection_patch_begin (kan_reflection_patch_t patch);

/// \brief Returns iterator that points to the memory after the node chunk of built patch.
/// \details Keep in mind that during build process chunks can be optimized out and compressed.
REFLECTION_API kan_reflection_patch_iterator_t kan_reflection_patch_end (kan_reflection_patch_t patch);

/// \brief Moves patch node iterator to the next patch node.
REFLECTION_API kan_reflection_patch_iterator_t
kan_reflection_patch_iterator_next (kan_reflection_patch_iterator_t iterator);

/// \brief Returns info about node to which iterator is pointing right now.
REFLECTION_API struct kan_reflection_patch_node_info_t kan_reflection_patch_iterator_get (
    kan_reflection_patch_iterator_t iterator);

/// \brief Destroys given patch.
REFLECTION_API void kan_reflection_patch_destroy (kan_reflection_patch_t patch);

KAN_C_HEADER_END
