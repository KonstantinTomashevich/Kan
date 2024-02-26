#pragma once

#include <readable_data_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/stream/stream.h>

/// \file
/// \brief Contains API for parsing and emitting readable data format.
///
/// \par Grammar
/// \parblock
/// Grammar bellow is more in read-to-understand specification than formal math one.
///
/// ```
/// identifier ::= [A-Za-z_][A-Za-z0-9_\.]*
/// array_index ::= \[[0-9]+\]
/// output_target ::= identifier array_index?
///
/// string_literal ::= ".*" (".*")*
/// integer_literal ::= (+ | -)? [0-9]+
/// floating_literal ::= (+ | -)? [0-9]*\.[0-9]+
///
/// value_identifier ::= identifier (, identifier)*
/// value_string ::= string_literal (, string_literal)*
/// value_integer :: integer_literal (, integer_literal)*
/// value_floating :: floating_literal (, floating_literal)*
/// value ::= value_identifier | value_string | value_integer | value_floating
///
/// elemental_setter ::= output_target = value
/// structural_setter ::= output_target { any_setter* }
/// array_appender ::= +output_target { any_setter* }
/// any_setter ::= elemental_setter | structural_setter | array_appender
///
/// comment ::= // .* \n
///
/// document ::= (comment | any_setter)+
/// ```
/// \endparblock

// TODO: Docs.

KAN_C_HEADER_BEGIN

enum kan_readable_data_event_type_t
{
    KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER,
    KAN_READABLE_DATA_EVENT_ELEMENTAL_STRING_SETTER,
    KAN_READABLE_DATA_EVENT_ELEMENTAL_INTEGER_SETTER,
    KAN_READABLE_DATA_EVENT_ELEMENTAL_FLOATING_SETTER,
    KAN_READABLE_DATA_EVENT_STRUCTURAL_SETTER_BEGIN,
    KAN_READABLE_DATA_EVENT_ARRAY_APPENDER_BEGIN,
    KAN_READABLE_DATA_EVENT_BLOCK_END,
};

#define KAN_READABLE_DATA_ARRAY_INDEX_NONE UINT64_MAX

struct kan_readable_data_output_target_t
{
    const char *identifier;
    uint64_t array_index;
};

struct kan_readable_data_identifier_node_t
{
    struct kan_readable_data_identifier_node_t *next;
    const char *identifier;
};

struct kan_readable_data_string_node_t
{
    struct kan_readable_data_string_node_t *next;
    const char *string;
};

struct kan_readable_data_integer_node_t
{
    struct kan_readable_data_integer_node_t *next;
    int64_t integer;
};

struct kan_readable_data_floating_node_t
{
    struct kan_readable_data_floating_node_t *next;
    double floating;
};

struct kan_readable_data_event_t
{
    enum kan_readable_data_event_type_t type;
    struct kan_readable_data_output_target_t output_target;

    union
    {
        struct kan_readable_data_identifier_node_t *setter_value_first_identifier;
        struct kan_readable_data_string_node_t *setter_value_first_string;
        struct kan_readable_data_integer_node_t *setter_value_first_integer;
        struct kan_readable_data_floating_node_t *setter_value_first_floating;
    };
};

enum kan_readable_data_parser_response_t
{
    KAN_READABLE_DATA_PARSER_RESPONSE_NEW_EVENT,
    KAN_READABLE_DATA_PARSER_RESPONSE_FAILED,
    KAN_READABLE_DATA_PARSER_RESPONSE_COMPLETED,
};

typedef uint64_t kan_readable_data_parser_t;

READABLE_DATA_API kan_readable_data_parser_t kan_readable_data_parser_create (struct kan_stream_t *input_stream);

READABLE_DATA_API enum kan_readable_data_parser_response_t kan_readable_data_parser_step (
    kan_readable_data_parser_t parser);

READABLE_DATA_API const struct kan_readable_data_event_t *kan_readable_data_parser_get_last_event (
    kan_readable_data_parser_t parser);

READABLE_DATA_API void kan_readable_data_parser_destroy (kan_readable_data_parser_t parser);

typedef uint64_t kan_readable_data_emitter_t;

READABLE_DATA_API kan_readable_data_emitter_t kan_readable_data_emitter_create (struct kan_stream_t *output_stream);

READABLE_DATA_API kan_bool_t kan_readable_data_emitter_step (kan_readable_data_emitter_t emitter,
                                                             struct kan_readable_data_event_t *emit_event);

READABLE_DATA_API void kan_readable_data_emitter_destroy (kan_readable_data_emitter_t emitter);

KAN_C_HEADER_END
