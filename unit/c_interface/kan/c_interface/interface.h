#pragma once

#include <c_interface_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/container/interned_string.h>
#include <kan/memory_profiler/allocation_group.h>
#include <kan/stream/stream.h>

/// \file
/// \brief Describes data structures that store info about scanned C code interface and provides ability to
///        serialize and deserialize this data.
///
/// \par Meta
/// \parblock
/// In addition to storing information about all enums, structs, functions and symbols, interface structure stores
/// user-defined meta that can be used by other tools to analyze C interface. Meta is expected to be added inside
/// comments with `\\meta` prefix before meta target appears, for example:
///
/// ```c
/// // \meta first_function_meta_as_marker
/// // \meta second_marker_meta_as_string = "second"
/// // \meta third_marker_meta_as_string_without_spaces = it_is_still_okay
/// void my_function (/* \meta argument_meta_as_int = 42 */ int first_arg, /* \meta second_arg_meta */ int second_arg);
/// ```
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Enumerates supported meta types.
enum kan_c_meta_type_t
{
    KAN_C_META_MARKER = 0u,
    KAN_C_META_INTEGER,
    KAN_C_META_STRING,
};

/// \brief Contains information about some meta added to declaration.
struct kan_c_meta_t
{
    enum kan_c_meta_type_t type;
    kan_interned_string_t name;

    union
    {
        int64_t integer_value;
        kan_interned_string_t string_value;
    };
};

/// \brief Container for all meta that is added to some declaration.
struct kan_c_meta_attachment_t
{
    uint64_t meta_count;
    struct kan_c_meta_t *meta_array;
};

/// \brief Contains information about enumeration value.
struct kan_c_enum_value_t
{
    kan_interned_string_t name;
    struct kan_c_meta_attachment_t meta;
};

/// \brief Contains information about enumeration.
struct kan_c_enum_t
{
    kan_interned_string_t name;
    uint64_t values_count;
    struct kan_c_enum_value_t *values;
    struct kan_c_meta_attachment_t meta;
};

/// \brief Enumerates supported type archetypes.
enum kan_c_archetype_t
{
    KAN_C_ARCHETYPE_BASIC = 0u,
    KAN_C_ARCHETYPE_ENUM,
    KAN_C_ARCHETYPE_STRUCT,
};

/// \brief Contains information about type, for example field type or return type.
struct kan_c_type_t
{
    kan_interned_string_t name;
    enum kan_c_archetype_t archetype;

    kan_bool_t is_const;
    kan_bool_t is_array;
    uint8_t pointer_level;
};

/// \brief Contains information about some variable, for example symbol or function argument.
struct kan_c_variable_t
{
    kan_interned_string_t name;
    struct kan_c_type_t type;
    struct kan_c_meta_attachment_t meta;
};

/// \brief Contains information about structure.
struct kan_c_struct_t
{
    kan_interned_string_t name;
    uint64_t fields_count;
    struct kan_c_variable_t *fields;
    struct kan_c_meta_attachment_t meta;
};

/// \brief Contains information about function.
struct kan_c_function_t
{
    kan_interned_string_t name;
    struct kan_c_type_t return_type;
    uint64_t arguments_count;
    struct kan_c_variable_t *arguments;
    struct kan_c_meta_attachment_t meta;
};

/// \brief Contains information about all enumerations, structures, exported functions and exported symbols.
struct kan_c_interface_t
{
    uint64_t enums_count;
    struct kan_c_enum_t *enums;

    uint64_t structs_count;
    struct kan_c_struct_t *structs;

    uint64_t functions_count;
    struct kan_c_function_t *functions;

    uint64_t symbols_count;
    struct kan_c_variable_t *symbols;
};

/// \brief Returns allocation group used to allocate everything connected to interface usage.
/// \details Aimed to be used inside `c_interface`, but is allowed to be used outside.
C_INTERFACE_API kan_allocation_group_t kan_c_interface_allocation_group (void);

/// \brief Serializes given interface into given stream.
C_INTERFACE_API kan_bool_t kan_c_interface_serialize (const struct kan_c_interface_t *interface,
                                                      struct kan_stream_t *stream);

/// \brief Deserializes interface from given stream, returns `NULL` on error.
C_INTERFACE_API struct kan_c_interface_t *kan_c_interface_deserialize (struct kan_stream_t *stream);

/// \brief Destroys given interface and frees all the resources.
C_INTERFACE_API void kan_c_interface_destroy (struct kan_c_interface_t *interface);

KAN_C_HEADER_END
