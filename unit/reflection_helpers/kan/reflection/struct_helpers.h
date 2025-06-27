#pragma once

#include <reflection_helpers_api.h>

#include <kan/api_common/c_header.h>
#include <kan/hash/hash.h>
#include <kan/reflection/registry.h>

/// \file
/// \brief Provides helpers for working with structures through reflection.

KAN_C_HEADER_BEGIN

/// \brief Extracts inline array size using size field if provided in reflection data.
REFLECTION_HELPERS_API kan_instance_size_t kan_reflection_get_inline_array_size (
    const struct kan_reflection_field_t *array_field, const void *owner_struct_instance);

/// \brief Hashes all visible fields including child structures and dynamic arrays.
/// \invariant There is no fields of KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER and
///            KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER archetypes.
REFLECTION_HELPERS_API kan_hash_t kan_reflection_hash_struct (kan_reflection_registry_t registry,
                                                              const struct kan_reflection_struct_t *type,
                                                              const void *instance);

/// \brief Compares two structures by all visible fields one-by-one, including child structures and dynamic arrays.
/// \invariant There is no fields of KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER and
///            KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER archetypes.
REFLECTION_HELPERS_API bool kan_reflection_are_structs_equal (kan_reflection_registry_t registry,
                                                              const struct kan_reflection_struct_t *type,
                                                              const void *first,
                                                              const void *second);

/// \brief Attempts to move data field-by-field from source struct to target one.
/// \invariant Target points to freshly allocated and initialized struct with zero capacity arrays,
///            otherwise old data will be leaked.
/// \invariant There is no fields of KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER and
///            KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER archetypes.
/// \details KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL, KAN_REFLECTION_ARCHETYPE_STRING_POINTER,
///          KAN_REFLECTION_ARCHETYPE_PATCH fields in source struct are nullified as a result of move.
///          KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY fields data is moved to target struct and arrays in source struct
///          are properly detached from that data and reset to zero size.
///          Other supported archetypes are just copied.
REFLECTION_HELPERS_API void kan_reflection_move_struct (kan_reflection_registry_t registry,
                                                        const struct kan_reflection_struct_t *type,
                                                        void *target,
                                                        void *source);

/// \brief Provides simplistic common mechanism for struct data reset operation.
/// \invariant There is no KAN_REFLECTION_ARCHETYPE_STRING_POINTER, KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER and
///            KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER as it is not entirely obvious what to do with them: deallocate
///            them or leave them be.
/// \details KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL fields are reset to invalid (zero) state.
///          KAN_REFLECTION_ARCHETYPE_PATCH instances are deployed and replaces with invalid patches.
///          KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY fields are resized to zero size without dropping their allocation
///          and with proper shutdown of items if necessary.
REFLECTION_HELPERS_API void kan_reflection_reset_struct (kan_reflection_registry_t registry,
                                                         const struct kan_reflection_struct_t *type,
                                                         void *instance);

KAN_C_HEADER_END
