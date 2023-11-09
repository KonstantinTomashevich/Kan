#pragma once

#include <c_interface_api.h>

#include <kan/api_common/c_header.h>
#include <kan/c_interface/interface.h>

/// \file
/// \brief Contains utility for building C interface from list of high level tokens. Useful for scanners.

KAN_C_HEADER_BEGIN

/// \brief Enumerates supported types of high level tokens.
enum kan_c_token_type_t
{
    KAN_C_TOKEN_MARKER_META = 0u,
    KAN_C_TOKEN_INTEGER_META,
    KAN_C_TOKEN_STRING_META,
    KAN_C_TOKEN_ENUM_VALUE,
    KAN_C_TOKEN_ENUM_BEGIN,
    KAN_C_TOKEN_ENUM_END,
    KAN_C_TOKEN_STRUCT_FIELD,
    KAN_C_TOKEN_STRUCT_BEGIN,
    KAN_C_TOKEN_STRUCT_END,
    KAN_C_TOKEN_FUNCTION_ARGUMENT,
    KAN_C_TOKEN_FUNCTION_BEGIN,
    KAN_C_TOKEN_FUNCTION_END,
    KAN_C_TOKEN_SYMBOL,
};

struct kan_c_token_marker_meta_t
{
    kan_interned_string_t name;
};

struct kan_c_token_integer_meta_t
{
    kan_interned_string_t name;
    int64_t value;
};

struct kan_c_token_string_meta_t
{
    kan_interned_string_t name;
    kan_interned_string_t value;
};

struct kan_c_token_enum_value_t
{
    kan_interned_string_t name;
};

struct kan_c_token_enum_begin_t
{
    kan_interned_string_t name;
};

struct kan_c_token_struct_field_t
{
    kan_interned_string_t name;
    struct kan_c_type_t type;
};

struct kan_c_token_struct_begin_t
{
    kan_interned_string_t name;
};

struct kan_c_token_function_argument_t
{
    kan_interned_string_t name;
    struct kan_c_type_t type;
};

struct kan_c_token_function_begin_t
{
    kan_interned_string_t name;
    struct kan_c_type_t return_type;
};

struct kan_c_token_symbol_t
{
    kan_interned_string_t name;
    struct kan_c_type_t type;
};

/// \brief Contains high level token data and pointer to next high level token.
struct kan_c_token_t
{
    struct kan_c_token_t *next;
    enum kan_c_token_type_t type;

    union
    {
        struct kan_c_token_marker_meta_t marker_meta;
        struct kan_c_token_integer_meta_t integer_meta;
        struct kan_c_token_string_meta_t string_meta;
        struct kan_c_token_enum_value_t enum_value;
        struct kan_c_token_enum_begin_t enum_begin;
        struct kan_c_token_struct_field_t struct_field;
        struct kan_c_token_struct_begin_t struct_begin;
        struct kan_c_token_function_argument_t function_argument;
        struct kan_c_token_function_begin_t function_begin;
        struct kan_c_token_symbol_t symbol;
    };
};

/// \brief Builds C interface description from given sequence of high level tokens.
C_INTERFACE_API struct kan_c_interface_t *kan_c_interface_build (const struct kan_c_token_t *token_sequence);

/// \brief Utility for initializing enumerations array in interface.
/// \details Aimed to be used inside `c_interface`, but is allowed to be used outside.
C_INTERFACE_API void kan_c_interface_init_enums_array (struct kan_c_interface_t *interface, uint64_t count);

/// \brief Utility for initializing structures array in interface.
/// \details Aimed to be used inside `c_interface`, but is allowed to be used outside.
C_INTERFACE_API void kan_c_interface_init_structs_array (struct kan_c_interface_t *interface, uint64_t count);

/// \brief Utility for initializing functions array in interface.
/// \details Aimed to be used inside `c_interface`, but is allowed to be used outside.
C_INTERFACE_API void kan_c_interface_init_functions_array (struct kan_c_interface_t *interface, uint64_t count);

/// \brief Utility for initializing symbols array in interface.
/// \details Aimed to be used inside `c_interface`, but is allowed to be used outside.
C_INTERFACE_API void kan_c_interface_init_symbols_array (struct kan_c_interface_t *interface, uint64_t count);

/// \brief Utility for initializing values array in enumeration.
/// \details Aimed to be used inside `c_interface`, but is allowed to be used outside.
C_INTERFACE_API void kan_c_enum_init_values_array (struct kan_c_enum_t *enum_info, uint64_t count);

/// \brief Utility for initializing fields array in structure.
/// \details Aimed to be used inside `c_interface`, but is allowed to be used outside.
C_INTERFACE_API void kan_c_struct_init_fields_array (struct kan_c_struct_t *struct_info, uint64_t count);

/// \brief Utility for initializing arguments array in function.
/// \details Aimed to be used inside `c_interface`, but is allowed to be used outside.
C_INTERFACE_API void kan_c_function_init_arguments_array (struct kan_c_function_t *function_info, uint64_t count);

KAN_C_HEADER_END
