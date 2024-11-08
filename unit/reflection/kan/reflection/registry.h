#pragma once

#include <reflection_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/interned_string.h>

/// \file
/// \brief Provides ability to register and query enum, struct and function reflection data.
///
/// \par Reflection data structure
/// \parblock
/// Reflection data is described by public structures that are not dependent on implementation:
/// - `kan_reflection_enum_t`
/// - `kan_reflection_enum_value_t`
/// - `kan_reflection_struct_t`
/// - `kan_reflection_field_t`
/// - `kan_reflection_function_t`
/// - `kan_reflection_return_type_t`
/// - `kan_reflection_function_argument_t`
///
/// In addition to this, any runtime meta can be added to any reflection entry: enums, enum values, structs, struct
/// fields, functions and function arguments. Meta is basically a pair of `meta_type_name` interned string and an
/// arbitrary pointer, and you can attach several metas of one type to one entry. Keep in mind, that reflection meta is
/// not the same as `c_interface` meta: `c_interface` meta is designed to add info about interface during declaration,
/// while reflection meta aims to be registrable from outside. For example, transform structure should not specify
/// network details in its declaration as it is a common module that can be used in different projects with different
/// network settings, instead it should be added from outside in every project that uses transform and network modules.
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

typedef uint64_t kan_reflection_functor_user_data_t;
typedef void (*kan_reflection_initialize_functor) (kan_reflection_functor_user_data_t user_data, void *pointer);
typedef void (*kan_reflection_shutdown_functor) (kan_reflection_functor_user_data_t user_data, void *pointer);

/// \brief Describes fixed-size structure with optional initialize and shutdown functions.
struct kan_reflection_struct_t
{
    kan_interned_string_t name;
    uint64_t size;
    uint64_t alignment;
    kan_reflection_initialize_functor init;
    kan_reflection_shutdown_functor shutdown;
    kan_reflection_functor_user_data_t functor_user_data;
    uint64_t fields_count;

    /// \details Fields must be ordered by ascending offset.
    struct kan_reflection_field_t *fields;
};

/// \brief Describes function return type.
/// \details Archetypes KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY and
///          KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY are not supported.
struct kan_reflection_return_type_t
{
    /// \warning Special case: if size is zero, then return type is void.
    uint64_t size;

    enum kan_reflection_archetype_t archetype;

    union
    {
        struct kan_reflection_archetype_enum_suffix_t archetype_enum;
        struct kan_reflection_archetype_struct_suffix_t archetype_struct;
        struct kan_reflection_archetype_struct_suffix_t archetype_struct_pointer;
    };
};

/// \brief Describes function argument.
/// \details Archetypes KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY and
///          KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY are not supported.
struct kan_reflection_argument_t
{
    kan_interned_string_t name;
    uint64_t size;
    enum kan_reflection_archetype_t archetype;

    union
    {
        struct kan_reflection_archetype_enum_suffix_t archetype_enum;
        struct kan_reflection_archetype_struct_suffix_t archetype_struct;
        struct kan_reflection_archetype_struct_suffix_t archetype_struct_pointer;
    };
};

typedef void (*kan_reflection_call_functor) (kan_reflection_functor_user_data_t user_data,
                                             void *return_address,
                                             void *arguments_address);

/// \brief Describes callable reflected function.
struct kan_reflection_function_t
{
    kan_interned_string_t name;
    kan_reflection_call_functor call;
    kan_reflection_functor_user_data_t call_user_data;

    struct kan_reflection_return_type_t return_type;
    uint64_t arguments_count;
    struct kan_reflection_argument_t *arguments;
};

/// \brief Claims memory for enum meta iterator implementation.
struct kan_reflection_enum_meta_iterator_t
{
    uint64_t implementation_data[4u];
};

/// \brief Claims memory for enum value meta iterator implementation.
struct kan_reflection_enum_value_meta_iterator_t
{
    uint64_t implementation_data[5u];
};

/// \brief Claims memory for struct meta iterator implementation.
struct kan_reflection_struct_meta_iterator_t
{
    uint64_t implementation_data[4u];
};

/// \brief Claims memory for struct field meta iterator implementation.
struct kan_reflection_struct_field_meta_iterator_t
{
    uint64_t implementation_data[5u];
};

/// \brief Claims memory for function meta iterator implementation.
struct kan_reflection_function_meta_iterator_t
{
    uint64_t implementation_data[4u];
};

/// \brief Claims memory for function argument meta iterator implementation.
struct kan_reflection_function_argument_meta_iterator_t
{
    uint64_t implementation_data[5u];
};

KAN_HANDLE_DEFINE (kan_reflection_registry_t);

/// \brief Allocates new reflection registry.
REFLECTION_API kan_reflection_registry_t kan_reflection_registry_create (void);

/// \brief Adds new enum unless its type name is already taken.
REFLECTION_API kan_bool_t kan_reflection_registry_add_enum (kan_reflection_registry_t registry,
                                                            const struct kan_reflection_enum_t *enum_reflection);

/// \brief Adds meta of given type to given enum.
REFLECTION_API void kan_reflection_registry_add_enum_meta (kan_reflection_registry_t registry,
                                                           kan_interned_string_t enum_name,
                                                           kan_interned_string_t meta_type_name,
                                                           const void *meta);

/// \brief Adds meta of given type to given value of given enum.
REFLECTION_API void kan_reflection_registry_add_enum_value_meta (kan_reflection_registry_t registry,
                                                                 kan_interned_string_t enum_name,
                                                                 kan_interned_string_t enum_value_name,
                                                                 kan_interned_string_t meta_type_name,
                                                                 const void *meta);

/// \brief Adds new struct unless its type name is already taken.
REFLECTION_API kan_bool_t kan_reflection_registry_add_struct (kan_reflection_registry_t registry,
                                                              const struct kan_reflection_struct_t *struct_reflection);

/// \brief Adds meta of given type to given struct.
REFLECTION_API void kan_reflection_registry_add_struct_meta (kan_reflection_registry_t registry,
                                                             kan_interned_string_t struct_name,
                                                             kan_interned_string_t meta_type_name,
                                                             const void *meta);

/// \brief Adds meta of given type to given field of given struct.
REFLECTION_API void kan_reflection_registry_add_struct_field_meta (kan_reflection_registry_t registry,
                                                                   kan_interned_string_t struct_name,
                                                                   kan_interned_string_t struct_field_name,
                                                                   kan_interned_string_t meta_type_name,
                                                                   const void *meta);

/// \brief Adds new function unless its name is already taken.
REFLECTION_API kan_bool_t kan_reflection_registry_add_function (
    kan_reflection_registry_t registry, const struct kan_reflection_function_t *function_reflection);

/// \brief Adds meta of given type to given function.
REFLECTION_API void kan_reflection_registry_add_function_meta (kan_reflection_registry_t registry,
                                                               kan_interned_string_t function_name,
                                                               kan_interned_string_t meta_type_name,
                                                               const void *meta);

/// \brief Adds meta of given type to given argument of given function.
REFLECTION_API void kan_reflection_registry_add_function_argument_meta (kan_reflection_registry_t registry,
                                                                        kan_interned_string_t function_name,
                                                                        kan_interned_string_t function_argument_name,
                                                                        kan_interned_string_t meta_type_name,
                                                                        const void *meta);

/// \brief Queries for enum by its name.
REFLECTION_API const struct kan_reflection_enum_t *kan_reflection_registry_query_enum (
    kan_reflection_registry_t registry, kan_interned_string_t enum_name);

/// \brief Queries for enum meta and returns result iterator.
REFLECTION_API struct kan_reflection_enum_meta_iterator_t kan_reflection_registry_query_enum_meta (
    kan_reflection_registry_t registry, kan_interned_string_t enum_name, kan_interned_string_t meta_type_name);

/// \brief Returns pointer to meta object or `NULL` if there is no more meta.
REFLECTION_API const void *kan_reflection_enum_meta_iterator_get (struct kan_reflection_enum_meta_iterator_t *iterator);

/// \brief Moves iterator to the next meta unless it already points to the end.
REFLECTION_API void kan_reflection_enum_meta_iterator_next (struct kan_reflection_enum_meta_iterator_t *iterator);

/// \brief Queries for enum value meta and returns result iterator.
REFLECTION_API struct kan_reflection_enum_value_meta_iterator_t kan_reflection_registry_query_enum_value_meta (
    kan_reflection_registry_t registry,
    kan_interned_string_t enum_name,
    kan_interned_string_t enum_value_name,
    kan_interned_string_t meta_type_name);

/// \brief Returns pointer to meta object or `NULL` if there is no more meta.
REFLECTION_API const void *kan_reflection_enum_value_meta_iterator_get (
    struct kan_reflection_enum_value_meta_iterator_t *iterator);

/// \brief Moves iterator to the next meta unless it already points to the end.
REFLECTION_API void kan_reflection_enum_value_meta_iterator_next (
    struct kan_reflection_enum_value_meta_iterator_t *iterator);

/// \brief Queries for struct by its name.
REFLECTION_API const struct kan_reflection_struct_t *kan_reflection_registry_query_struct (
    kan_reflection_registry_t registry, kan_interned_string_t struct_name);

/// \brief Queries for struct meta and returns result iterator.
REFLECTION_API struct kan_reflection_struct_meta_iterator_t kan_reflection_registry_query_struct_meta (
    kan_reflection_registry_t registry, kan_interned_string_t struct_name, kan_interned_string_t meta_type_name);

/// \brief Returns pointer to meta object or `NULL` if there is no more meta.
REFLECTION_API const void *kan_reflection_struct_meta_iterator_get (
    struct kan_reflection_struct_meta_iterator_t *iterator);

/// \brief Moves iterator to the next meta unless it already points to the end.
REFLECTION_API void kan_reflection_struct_meta_iterator_next (struct kan_reflection_struct_meta_iterator_t *iterator);

/// \brief Queries for struct field meta and returns result iterator.
REFLECTION_API struct kan_reflection_struct_field_meta_iterator_t kan_reflection_registry_query_struct_field_meta (
    kan_reflection_registry_t registry,
    kan_interned_string_t struct_name,
    kan_interned_string_t struct_field_name,
    kan_interned_string_t meta_type_name);

/// \brief Returns pointer to meta object or `NULL` if there is no more meta.
REFLECTION_API const void *kan_reflection_struct_field_meta_iterator_get (
    struct kan_reflection_struct_field_meta_iterator_t *iterator);

/// \brief Moves iterator to the next meta unless it already points to the end.
REFLECTION_API void kan_reflection_struct_field_meta_iterator_next (
    struct kan_reflection_struct_field_meta_iterator_t *iterator);

/// \brief Queries for function by its name.
REFLECTION_API const struct kan_reflection_function_t *kan_reflection_registry_query_function (
    kan_reflection_registry_t registry, kan_interned_string_t function_name);

/// \brief Queries for function meta and returns result iterator.
REFLECTION_API struct kan_reflection_function_meta_iterator_t kan_reflection_registry_query_function_meta (
    kan_reflection_registry_t registry, kan_interned_string_t function_name, kan_interned_string_t meta_type_name);

/// \brief Returns pointer to meta object or `NULL` if there is no more meta.
REFLECTION_API const void *kan_reflection_function_meta_iterator_get (
    struct kan_reflection_function_meta_iterator_t *iterator);

/// \brief Moves iterator to the next meta unless it already points to the end.
REFLECTION_API void kan_reflection_function_meta_iterator_next (
    struct kan_reflection_function_meta_iterator_t *iterator);

/// \brief Queries for function argument meta and returns result iterator.
REFLECTION_API struct kan_reflection_function_argument_meta_iterator_t
kan_reflection_registry_query_function_argument_meta (kan_reflection_registry_t registry,
                                                      kan_interned_string_t function_name,
                                                      kan_interned_string_t function_argument_name,
                                                      kan_interned_string_t meta_type_name);

/// \brief Returns pointer to meta object or `NULL` if there is no more meta.
REFLECTION_API const void *kan_reflection_function_argument_meta_iterator_get (
    struct kan_reflection_function_argument_meta_iterator_t *iterator);

/// \brief Moves iterator to the next meta unless it already points to the end.
REFLECTION_API void kan_reflection_function_argument_meta_iterator_next (
    struct kan_reflection_function_argument_meta_iterator_t *iterator);

/// \brief Queries for field in fixed memory block by path array.
/// \details Struct name parameter decides search root for the path and path is an array of field names in succession.
///          You can imagine structure reflection as a tree and path is a path from root to target field node.
///          Query only succeeds if target field is local for given struct: it means that it is stored in the
///          same memory block.
REFLECTION_API const struct kan_reflection_field_t *kan_reflection_registry_query_local_field (
    kan_reflection_registry_t registry,
    kan_interned_string_t struct_name,
    uint64_t path_length,
    kan_interned_string_t *path,
    uint64_t *absolute_offset_output,
    uint64_t *size_with_padding_output);

KAN_HANDLE_DEFINE (kan_reflection_registry_enum_iterator_t);

/// \brief Returns iterator that points to the beginning of enums storage.
REFLECTION_API kan_reflection_registry_enum_iterator_t
kan_reflection_registry_enum_iterator_create (kan_reflection_registry_t registry);

/// \brief Returns enum to which iterator points or `NULL` if there is no more enums.
REFLECTION_API const struct kan_reflection_enum_t *kan_reflection_registry_enum_iterator_get (
    kan_reflection_registry_enum_iterator_t iterator);

/// \brief Moves iterator to the next enum unless it already points to the end.
REFLECTION_API kan_reflection_registry_enum_iterator_t
kan_reflection_registry_enum_iterator_next (kan_reflection_registry_enum_iterator_t iterator);

KAN_HANDLE_DEFINE (kan_reflection_registry_struct_iterator_t);

/// \brief Returns iterator that points to the beginning of structs storage.
REFLECTION_API kan_reflection_registry_struct_iterator_t
kan_reflection_registry_struct_iterator_create (kan_reflection_registry_t registry);

/// \brief Returns struct to which iterator points or `NULL` if there is no more structs.
REFLECTION_API const struct kan_reflection_struct_t *kan_reflection_registry_struct_iterator_get (
    kan_reflection_registry_struct_iterator_t iterator);

/// \brief Moves iterator to the next struct unless it already points to the end.
REFLECTION_API kan_reflection_registry_struct_iterator_t
kan_reflection_registry_struct_iterator_next (kan_reflection_registry_struct_iterator_t iterator);

KAN_HANDLE_DEFINE (kan_reflection_registry_function_iterator_t);

/// \brief Returns iterator that points to the beginning of functions storage.
REFLECTION_API kan_reflection_registry_function_iterator_t
kan_reflection_registry_function_iterator_create (kan_reflection_registry_t registry);

/// \brief Returns function to which iterator points or `NULL` if there is no more structs.
REFLECTION_API const struct kan_reflection_function_t *kan_reflection_registry_function_iterator_get (
    kan_reflection_registry_function_iterator_t iterator);

/// \brief Moves iterator to the next function unless it already points to the end.
REFLECTION_API kan_reflection_registry_function_iterator_t
kan_reflection_registry_function_iterator_next (kan_reflection_registry_function_iterator_t iterator);

/// \brief Destroys reflection registry. Does not destroy registered reflection data.
REFLECTION_API void kan_reflection_registry_destroy (kan_reflection_registry_t registry);

// TODO: Support for reflection remaps like Unreal Engine core redirects? Might be needed later.

KAN_C_HEADER_END
