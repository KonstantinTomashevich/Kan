#pragma once

#include <reflection_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/container/interned_string.h>

/// \file
/// \brief Provides ability to register and query struct and enum reflection data.
///
/// \par Reflection data structure
/// \parblock
/// Reflection data is described by public structures that are not dependent on implementation:
/// - `kan_reflection_enum_t`
/// - `kan_reflection_enum_value_t`
/// - `kan_reflection_struct_t`
/// - `kan_reflection_field_t`
///
/// In addition to this, any runtime meta can be added to any reflection entry: enums, enum values, structs and struct
/// fields. Meta is basically a pair of `meta_type_name` interned string and an arbitrary pointer. The only restriction
/// is that `meta_type_name` functions as key, therefore there can't be several metas with the same `meta_type_name` on
/// single entry. Keep in mind, that reflection meta is not the same as `c_interface` meta: `c_interface` meta is
/// designed to add info about interface during declaration, while reflection meta aims to be registrable from outside.
/// For example, transform structure should not specify network details in its declaration as it is common module that
/// can be used in different projects with different network settings, instead it should be added from outside in every
/// project that uses transform and network modules.
/// \endparblock
///
/// \par Registry
/// \parblock
/// Registry serves as container for reflection data. Registry supports data addition, querying and iteration. The main
/// goal of registry is to unite reflection data into single context that can be used by other modules. Keep in mind
/// that registry does not control lifetime of reflection data, therefore reflection data should be manually disposed
/// when it is no longer needed (if it was allocated dynamically, for example from scripts in assets).
/// \endparblock
///
/// \par Field visibility
/// \parblock
/// Some fields technically have so-called dependent visibility: they only make sense when other field value is equal
/// to some constants. For example, there are multiple fields inside union, but only one at a time makes sense. This
/// is indicated by `visibility_condition_*` fields of `kan_reflection_field_t`. If `visibility_condition_field` is
/// `NULL`, then field is considered always visible. Otherwise, value of `visibility_condition_field` will be checked
/// by visibility-dependent code like serializers.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Describes single enumeration value.
struct kan_reflection_enum_value_t
{
    kan_interned_string_t name;
    int64_t value;
};

/// \brief Describes single enumeration with its values.
struct kan_reflection_enum_t
{
    kan_interned_string_t name;

    /// \brief If true, enumeration should be treated as set of flags instead of select-single enumeration.
    kan_bool_t flags;

    uint64_t values_count;
    struct kan_reflection_enum_value_t *values;
};

/// \brief Field types are separated into archetypes for easier processing.
enum kan_reflection_archetype_t
{
    /// \brief Depending on size, either int8_t, int16_t, int32_t or int64_t.
    KAN_REFLECTION_ARCHETYPE_SIGNED_INT = 0u,

    /// \brief Depending on size, either uint8_t, uint16_t, uint32_t or uint64_t.
    KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,

    /// \brief Depending on size, either float or double
    KAN_REFLECTION_ARCHETYPE_FLOATING,

    /// \brief Pointer to string that is not interned.
    KAN_REFLECTION_ARCHETYPE_STRING_POINTER,

    /// \brief Refers to `kan_interned_string_t` instance.
    KAN_REFLECTION_ARCHETYPE_INTERNED_STRING,

    /// \brief Refers to a value that belongs to some enumeration.
    KAN_REFLECTION_ARCHETYPE_ENUM,

    /// \brief Refers to a pointer to arbitrary data type that is not registered as struct in reflection.
    KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER,

    /// \brief Refers to a structure that is a part of main structure memory block.
    KAN_REFLECTION_ARCHETYPE_STRUCT,

    /// \brief Refers to a pointer to some structure.
    KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER,

    /// \brief Refers to fixed size array that is a part of structure memory block.
    /// \invariant Multidimensional inline arrays are not supported.
    KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY,

    /// \brief Refers to instance of `kan_dynamic_array_t`.
    KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY,

    /// \brief Refers to reflection-driven patch instance.
    KAN_REFLECTION_ARCHETYPE_PATCH,
};

/// \brief Additional field info for KAN_REFLECTION_ARCHETYPE_ENUM fields.
struct kan_reflection_archetype_enum_suffix_t
{
    kan_interned_string_t type_name;
};

/// \brief Additional field info for KAN_REFLECTION_ARCHETYPE_STRUCT and KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER fields.
struct kan_reflection_archetype_struct_suffix_t
{
    kan_interned_string_t type_name;
};

/// \brief Additional field info for KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY fields.
/// \details Item archetypes KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY and KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY are not
///          supported.
struct kan_reflection_archetype_inline_array_suffix_t
{
    enum kan_reflection_archetype_t item_archetype;
    uint64_t item_size;

    union
    {
        struct kan_reflection_archetype_enum_suffix_t item_archetype_enum;
        struct kan_reflection_archetype_struct_suffix_t item_archetype_struct;
        struct kan_reflection_archetype_struct_suffix_t item_archetype_struct_pointer;
    };

    uint64_t item_count;
    const struct kan_reflection_field_t *size_field;
};

/// \brief Additional field info for KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY fields.
/// \details Item archetypes KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY and KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY are not
///          supported.
struct kan_reflection_archetype_dynamic_array_suffix_t
{
    enum kan_reflection_archetype_t item_archetype;
    uint64_t item_size;

    union
    {
        struct kan_reflection_archetype_enum_suffix_t item_archetype_enum;
        struct kan_reflection_archetype_struct_suffix_t item_archetype_struct;
        struct kan_reflection_archetype_struct_suffix_t item_archetype_struct_pointer;
    };
};

/// \brief Describes field of a structure.
struct kan_reflection_field_t
{
    kan_interned_string_t name;
    uint64_t offset;
    uint64_t size;
    enum kan_reflection_archetype_t archetype;

    union
    {
        struct kan_reflection_archetype_enum_suffix_t archetype_enum;
        struct kan_reflection_archetype_struct_suffix_t archetype_struct;
        struct kan_reflection_archetype_struct_suffix_t archetype_struct_pointer;
        struct kan_reflection_archetype_inline_array_suffix_t archetype_inline_array;
        struct kan_reflection_archetype_dynamic_array_suffix_t archetype_dynamic_array;
    };

    struct kan_reflection_field_t *visibility_condition_field;
    uint64_t visibility_condition_values_count;
    int64_t *visibility_condition_values;
};

typedef void (*kan_reflection_initialize_functor) (void *pointer);
typedef void (*kan_reflection_shutdown_functor) (void *pointer);

/// \brief Describes fixed-size structure with optional initialize and shutdown functions.
struct kan_reflection_struct_t
{
    kan_interned_string_t name;
    uint64_t size;
    uint64_t alignment;
    kan_reflection_initialize_functor init;
    kan_reflection_shutdown_functor shutdown;
    uint64_t fields_count;
    struct kan_reflection_field_t *fields;
};

typedef uint64_t kan_reflection_registry_t;

/// \brief Allocates new reflection registry.
REFLECTION_API kan_reflection_registry_t kan_reflection_registry_create (void);

/// \brief Adds new enum unless its type name is already taken.
REFLECTION_API kan_bool_t kan_reflection_registry_add_enum (kan_reflection_registry_t registry,
                                                            const struct kan_reflection_enum_t *enum_reflection);

/// \brief Adds new enum meta unless its meta type name is already taken.
REFLECTION_API kan_bool_t kan_reflection_registry_add_enum_meta (kan_reflection_registry_t registry,
                                                                 kan_interned_string_t enum_name,
                                                                 kan_interned_string_t meta_type_name,
                                                                 const void *meta);

/// \brief Adds new enum value meta unless its meta type name is already taken.
REFLECTION_API kan_bool_t kan_reflection_registry_add_enum_value_meta (kan_reflection_registry_t registry,
                                                                       kan_interned_string_t enum_name,
                                                                       kan_interned_string_t enum_value_name,
                                                                       kan_interned_string_t meta_type_name,
                                                                       const void *meta);

/// \brief Adds new struct unless its type name is already taken.
REFLECTION_API kan_bool_t kan_reflection_registry_add_struct (kan_reflection_registry_t registry,
                                                              const struct kan_reflection_struct_t *struct_reflection);

/// \brief Adds new struct meta unless its meta type name is already taken.
REFLECTION_API kan_bool_t kan_reflection_registry_add_struct_meta (kan_reflection_registry_t registry,
                                                                   kan_interned_string_t struct_name,
                                                                   kan_interned_string_t meta_type_name,
                                                                   const void *meta);

/// \brief Adds new struct field meta unless its meta type name is already taken.
REFLECTION_API kan_bool_t kan_reflection_registry_add_struct_field_meta (kan_reflection_registry_t registry,
                                                                         kan_interned_string_t struct_name,
                                                                         kan_interned_string_t struct_field_name,
                                                                         kan_interned_string_t meta_type_name,
                                                                         const void *meta);

/// \brief Queries for enum by its name.
REFLECTION_API const struct kan_reflection_enum_t *kan_reflection_registry_query_enum (
    kan_reflection_registry_t registry, kan_interned_string_t enum_name);

/// \brief Queries for enum meta.
REFLECTION_API const void *kan_reflection_registry_query_enum_meta (kan_reflection_registry_t registry,
                                                                    kan_interned_string_t enum_name,
                                                                    kan_interned_string_t meta_type_name);

/// \brief Queries for enum value meta.
REFLECTION_API const void *kan_reflection_registry_query_enum_value_meta (kan_reflection_registry_t registry,
                                                                          kan_interned_string_t enum_name,
                                                                          kan_interned_string_t enum_value_name,
                                                                          kan_interned_string_t meta_type_name);

/// \brief Queries for struct by its name.
REFLECTION_API const struct kan_reflection_struct_t *kan_reflection_registry_query_struct (
    kan_reflection_registry_t registry, kan_interned_string_t struct_name);

/// \brief Queries for struct meta.
REFLECTION_API const void *kan_reflection_registry_query_struct_meta (kan_reflection_registry_t registry,
                                                                      kan_interned_string_t struct_name,
                                                                      kan_interned_string_t meta_type_name);

/// \brief Queries for struct field meta.
REFLECTION_API const void *kan_reflection_registry_query_struct_field_meta (kan_reflection_registry_t registry,
                                                                            kan_interned_string_t struct_name,
                                                                            kan_interned_string_t struct_field_name,
                                                                            kan_interned_string_t meta_type_name);

/// \brief Queries for field in fixed memory block by path array.
/// \details Path array is an array of 2 or more interned strings where first string is a struct name and subsequent
///          strings are field names. Query only succeeds if field is local for given struct: it means that it is stored
///          in the same memory block.
REFLECTION_API const struct kan_reflection_field_t *kan_reflection_registry_query_local_field (
    kan_reflection_registry_t registry,
    uint64_t path_length,
    kan_interned_string_t *path,
    uint64_t *absolute_offset_output);

typedef uint64_t kan_reflection_registry_enum_iterator_t;

/// \brief Returns iterator that points to the beginning of enums storage.
REFLECTION_API kan_reflection_registry_enum_iterator_t
kan_reflection_registry_enum_iterator_create (kan_reflection_registry_t registry);

/// \brief Returns enum to which iterator points or `NULL` if there is no more enums.
REFLECTION_API const struct kan_reflection_enum_t *kan_reflection_registry_enum_iterator_get (
    kan_reflection_registry_enum_iterator_t iterator);

/// \brief Moves iterator to the next enum unless it already points to the end.
REFLECTION_API kan_reflection_registry_enum_iterator_t
kan_reflection_registry_enum_iterator_next (kan_reflection_registry_enum_iterator_t iterator);

typedef uint64_t kan_reflection_registry_struct_iterator_t;

/// \brief Returns iterator that points to the beginning of structs storage.
REFLECTION_API kan_reflection_registry_struct_iterator_t
kan_reflection_registry_struct_iterator_create (kan_reflection_registry_t registry);

/// \brief Returns struct to which iterator points or `NULL` if there is no more structs.
REFLECTION_API const struct kan_reflection_struct_t *kan_reflection_registry_struct_iterator_get (
    kan_reflection_registry_struct_iterator_t iterator);

/// \brief Moves iterator to the next struct unless it already points to the end.
REFLECTION_API kan_reflection_registry_struct_iterator_t
kan_reflection_registry_struct_iterator_next (kan_reflection_registry_struct_iterator_t iterator);

/// \brief Destroys reflection registry. Does not destroy registered reflection data.
REFLECTION_API void kan_reflection_registry_destroy (kan_reflection_registry_t registry);

// TODO: Support for reflection remaps like Unreal Engine core redirects? Might be needed later.

KAN_C_HEADER_END
