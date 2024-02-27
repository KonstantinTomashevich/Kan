#pragma once

#include <readable_data_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/stream/stream.h>

/// \file
/// \brief Contains API for parsing and emitting readable data format.
///
/// \par Motivation
/// \parblock
/// Readable data is an in-house format for storing data structures as textual human-readable assets.
/// Initially, it was planed to use YAML or TOML, but, at the time of writing, there were following issues:
///
/// - libfyaml does not support Windows.
/// - libyaml was last updated 3 years ago.
/// - All other YAML parsers do not support per-event parsing and emitting.
/// - Official TOML parsers for C do not support per-event parsing and emitting too.
///
/// Therefore, it was decided to create internal textual serialization format that is:
/// - Easy to read and supports comments.
/// - Supports per-event parsing and emitting.
/// - Trivial to parse and emit.
/// - Supports multiple values per setter (for bitmasks and small arrays).
/// - Supports shortening by specifying whole path for setter ("my_outer.my_inner =" instead of
///   "my_outer { my_inner =").
/// \endparblock
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
/// document ::= (comment | any_setter)*
/// ```
/// \endparblock
///
/// \par Events
/// \parblock
/// Readable data format is build around idea of atomic events that describe one high-level parsing operation per event.
///
/// - KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER indicates operation of setting one or more identifiers to
///   target.
/// - KAN_READABLE_DATA_EVENT_ELEMENTAL_STRING_SETTER indicates operation of setting one or more strings to target.
/// - KAN_READABLE_DATA_EVENT_ELEMENTAL_INTEGER_SETTER indicates operation of setting one or more integers to target.
/// - KAN_READABLE_DATA_EVENT_ELEMENTAL_FLOATING_SETTER indicates operation of setting one or more floating point
///   numbers to target.
/// - KAN_READABLE_DATA_EVENT_STRUCTURAL_SETTER_BEGIN indicates that its output structure should be pushed on top of
///   output targets stack.
/// - KAN_READABLE_DATA_EVENT_ARRAY_APPENDER_BEGIN indicates that output target array should receive new item and this
///   item should be pushed on top of output targets stack.
/// - KAN_READABLE_DATA_EVENT_BLOCK_END indicates that output target stack top item needs to be popped out.
///
/// Using these principles it should be easy to implement high-level parser for readable data that parses events
/// directly into output data structures.
/// \endparblock
///
/// \par Parsing
/// \parblock
/// Parsing is done using `kan_readable_data_parser_t` and functions that take it as argument. For example:
///
/// ```c
/// // Create parse for parsing input stream.
/// kan_readable_data_parser_t parser = kan_readable_data_parser_create (input_data_stream);
/// enum kan_readable_data_parser_response_t response;
///
/// while ((response = kan_readable_data_parser_step (parser)) == KAN_READABLE_DATA_PARSER_RESPONSE_NEW_EVENT)
/// {
///     const struct kan_readable_data_event_t *new_event = kan_readable_data_parser_get_last_event (parser);
///     // Parse event here.
/// }
///
/// // Check response to see if data read is completed (KAN_READABLE_DATA_PARSER_RESPONSE_COMPLETED) or failed
/// // (KAN_READABLE_DATA_PARSER_RESPONSE_FAILED).
///
/// // Destroy parser after usage.
/// kan_readable_data_parser_destroy (parser);
/// ```
/// \endparblock
///
/// \par Emission
/// \parblock
/// Emission is done using `kan_readable_data_emitter_t` and functions that take it as argument. For example:
///
/// ```c
/// // Create emitter to emit data into output stream.
/// kan_readable_data_emitter_t emitter kan_readable_data_emitter_create (output_data_stream);
/// kan_bool_t emitted_successfully;
///
/// // Emit data as events.
/// do
/// {
///     struct kan_readable_data_event_t next_event;
///     // Fill next event or break if no more data.
/// }
/// while ((emitted_successfully = kan_readable_data_emitter_step (emitter, &next_event));
///
/// // Check emitted_successfully to see if all data was successfully emitted.
///
/// // Destroy emitter after usage.
/// kan_readable_data_emitter_destroy (emitter);
/// ```
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Describes event types for readable data format parsers and emitters.
enum kan_readable_data_event_type_t
{
    /// \brief Sets one or more values to output target in form of identifiers.
    KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER,

    /// \brief Sets one or more values to output target in form of strings.
    KAN_READABLE_DATA_EVENT_ELEMENTAL_STRING_SETTER,

    /// \brief Sets one or more values to output target in form of integers.
    KAN_READABLE_DATA_EVENT_ELEMENTAL_INTEGER_SETTER,

    /// \brief Sets one or more values to output target in form of floating point numbers.
    KAN_READABLE_DATA_EVENT_ELEMENTAL_FLOATING_SETTER,

    /// \brief Start new block for given output target.
    KAN_READABLE_DATA_EVENT_STRUCTURAL_SETTER_BEGIN,

    /// \brief Adds new item to given output target array and opens new block for added item.
    KAN_READABLE_DATA_EVENT_ARRAY_APPENDER_BEGIN,

    /// \brief Closes opened block.
    KAN_READABLE_DATA_EVENT_BLOCK_END,
};

/// \brief Value for kan_readable_data_output_target_t::array_index that specified absence of array index.
#define KAN_READABLE_DATA_ARRAY_INDEX_NONE UINT64_MAX

/// \brief Describes output target for setter or array appender.
struct kan_readable_data_output_target_t
{
    /// \brief Identifier that may contain dots as separators.
    const char *identifier;

    /// \brief Array index for identifier or KAN_READABLE_DATA_ARRAY_INDEX_NONE if not in array.
    uint64_t array_index;
};

/// \brief Value node for describing values for identifier setters.
struct kan_readable_data_identifier_node_t
{
    struct kan_readable_data_identifier_node_t *next;
    const char *identifier;
};

/// \brief Value node for describing values for string setters.
struct kan_readable_data_string_node_t
{
    struct kan_readable_data_string_node_t *next;
    const char *string;
};

/// \brief Value node for describing values for integer setters.
struct kan_readable_data_integer_node_t
{
    struct kan_readable_data_integer_node_t *next;
    int64_t integer;
};

/// \brief Value node for describing values for floating point number setters.
struct kan_readable_data_floating_node_t
{
    struct kan_readable_data_floating_node_t *next;
    double floating;
};

/// \brief Describes event for readable data format parsers and emitters.
struct kan_readable_data_event_t
{
    enum kan_readable_data_event_type_t type;

    /// \brief Output target is left uninitialized for KAN_READABLE_DATA_EVENT_BLOCK_END.
    struct kan_readable_data_output_target_t output_target;

    union
    {
        struct kan_readable_data_identifier_node_t *setter_value_first_identifier;
        struct kan_readable_data_string_node_t *setter_value_first_string;
        struct kan_readable_data_integer_node_t *setter_value_first_integer;
        struct kan_readable_data_floating_node_t *setter_value_first_floating;
    };
};

/// \brief Describes readable data format parser responses.
enum kan_readable_data_parser_response_t
{
    /// \brief New event is parsed and available.
    KAN_READABLE_DATA_PARSER_RESPONSE_NEW_EVENT,

    /// \brief Parsing failed and cannot be continued.
    KAN_READABLE_DATA_PARSER_RESPONSE_FAILED,

    /// \brief Everything from given stream is parsed now.
    KAN_READABLE_DATA_PARSER_RESPONSE_COMPLETED,
};

typedef uint64_t kan_readable_data_parser_t;

/// \brief Creates new instance of readable data parser.
READABLE_DATA_API kan_readable_data_parser_t kan_readable_data_parser_create (struct kan_stream_t *input_stream);

/// \brief Attempts to read next event from parser input stream.
READABLE_DATA_API enum kan_readable_data_parser_response_t kan_readable_data_parser_step (
    kan_readable_data_parser_t parser);

/// \brief Returns pointer to last read event. Invalidates previously read event.
READABLE_DATA_API const struct kan_readable_data_event_t *kan_readable_data_parser_get_last_event (
    kan_readable_data_parser_t parser);

/// \brief Destroys given parser instance.
READABLE_DATA_API void kan_readable_data_parser_destroy (kan_readable_data_parser_t parser);

typedef uint64_t kan_readable_data_emitter_t;

/// \brief Creates new readable data emitter instance.
READABLE_DATA_API kan_readable_data_emitter_t kan_readable_data_emitter_create (struct kan_stream_t *output_stream);

/// \brief Emits given event using given emitter.
READABLE_DATA_API kan_bool_t kan_readable_data_emitter_step (kan_readable_data_emitter_t emitter,
                                                             struct kan_readable_data_event_t *emit_event);

/// \brief Destroys given emitter instance.
READABLE_DATA_API void kan_readable_data_emitter_destroy (kan_readable_data_emitter_t emitter);

KAN_C_HEADER_END
