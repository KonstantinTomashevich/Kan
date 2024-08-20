#pragma once

#include <render_pipeline_language_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>

KAN_C_HEADER_BEGIN

enum kan_rpl_option_scope_t
{
    KAN_RPL_OPTION_SCOPE_GLOBAL = 0u,
    KAN_RPL_OPTION_SCOPE_INSTANCE,
};

enum kan_rpl_option_type_t
{
    KAN_RPL_OPTION_TYPE_FLAG = 0u,
    KAN_RPL_OPTION_TYPE_COUNT,
};

struct kan_rpl_option_t
{
    kan_interned_string_t name;
    enum kan_rpl_option_scope_t scope;
    enum kan_rpl_option_type_t type;

    union
    {
        /// \meta reflection_visibility_condition_field = "type"
        /// \meta reflection_visibility_condition_values = "(int64_t) KAN_RPL_OPTION_TYPE_FLAG"
        kan_bool_t flag_default_value;

        /// \meta reflection_visibility_condition_field = "type"
        /// \meta reflection_visibility_condition_values = "(int64_t) KAN_RPL_OPTION_TYPE_COUNT"
        uint64_t count_default_value;
    };
};

enum kan_rpl_binary_operation_t
{
    KAN_RPL_BINARY_OPERATION_FIELD_ACCESS = 0u,
    KAN_RPL_BINARY_OPERATION_ARRAY_ACCESS,
    KAN_RPL_BINARY_OPERATION_ADD,
    KAN_RPL_BINARY_OPERATION_SUBTRACT,
    KAN_RPL_BINARY_OPERATION_MULTIPLY,
    KAN_RPL_BINARY_OPERATION_DIVIDE,
    KAN_RPL_BINARY_OPERATION_MODULUS,
    KAN_RPL_BINARY_OPERATION_ASSIGN,
    KAN_RPL_BINARY_OPERATION_AND,
    KAN_RPL_BINARY_OPERATION_OR,
    KAN_RPL_BINARY_OPERATION_EQUAL,
    KAN_RPL_BINARY_OPERATION_NOT_EQUAL,
    KAN_RPL_BINARY_OPERATION_LESS,
    KAN_RPL_BINARY_OPERATION_GREATER,
    KAN_RPL_BINARY_OPERATION_LESS_OR_EQUAL,
    KAN_RPL_BINARY_OPERATION_GREATER_OR_EQUAL,
    KAN_RPL_BINARY_OPERATION_BITWISE_AND,
    KAN_RPL_BINARY_OPERATION_BITWISE_OR,
    KAN_RPL_BINARY_OPERATION_BITWISE_XOR,
    KAN_RPL_BINARY_OPERATION_BITWISE_LSHIFT,
    KAN_RPL_BINARY_OPERATION_BITWISE_RSHIFT,
};

enum kan_rpl_unary_operation_t
{
    KAN_RPL_UNARY_OPERATION_NEGATE = 0u,
    KAN_RPL_UNARY_OPERATION_NOT,
    KAN_RPL_UNARY_OPERATION_BITWISE_NOT,
};

enum kan_rpl_expression_node_type_t
{
    KAN_RPL_EXPRESSION_NODE_TYPE_NOPE = 0u,
    KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER,
    KAN_RPL_EXPRESSION_NODE_TYPE_INTEGER_LITERAL,
    KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL,
    KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION,
    KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION,
    KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION,
    KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE,
    KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL,
    KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR,
    KAN_RPL_EXPRESSION_NODE_TYPE_IF,
    KAN_RPL_EXPRESSION_NODE_TYPE_FOR,
    KAN_RPL_EXPRESSION_NODE_TYPE_WHILE,
    KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE,
    KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS,
    KAN_RPL_EXPRESSION_NODE_TYPE_BREAK,
    KAN_RPL_EXPRESSION_NODE_TYPE_CONTINUE,
    KAN_RPL_EXPRESSION_NODE_TYPE_RETURN,
};

struct kan_rpl_variable_declaration_data_t
{
    kan_interned_string_t type_name;
    kan_interned_string_t variable_name;
};

struct kan_rpl_expression_node_t
{
    enum kan_rpl_expression_node_type_t type;
    union
    {
        /// \meta reflection_visibility_condition_field = "type"
        /// \meta reflection_visibility_condition_values = "(int64_t) KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER"
        kan_interned_string_t identifier;

        /// \meta reflection_visibility_condition_field = "type"
        /// \meta reflection_visibility_condition_values = "(int64_t) KAN_RPL_EXPRESSION_NODE_TYPE_INTEGER_LITERAL"
        int64_t integer_literal;

        /// \meta reflection_visibility_condition_field = "type"
        /// \meta reflection_visibility_condition_values = "(int64_t) KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL"
        double floating_literal;

        /// \meta reflection_visibility_condition_field = "type"
        /// \meta reflection_visibility_condition_values = "(int64_t) KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION"
        struct kan_rpl_variable_declaration_data_t variable_declaration;

        /// \meta reflection_visibility_condition_field = "type"
        /// \meta reflection_visibility_condition_values = "(int64_t) KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION"
        enum kan_rpl_binary_operation_t binary_operation;

        /// \meta reflection_visibility_condition_field = "type"
        /// \meta reflection_visibility_condition_values = "(int64_t) KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION"
        enum kan_rpl_unary_operation_t unary_operation;

        /// \meta reflection_visibility_condition_field = "type"
        /// \meta reflection_visibility_condition_values = "(int64_t) KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL"
        kan_interned_string_t function_name;

        /// \meta reflection_visibility_condition_field = "type"
        /// \meta reflection_visibility_condition_values = "(int64_t) KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR"
        kan_interned_string_t constructor_type_name;

        /// \meta reflection_visibility_condition_field = "type"
        /// \meta reflection_visibility_condition_values = "(int64_t) KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS"
        kan_interned_string_t alias_name;
    };

    /// \brief Array of children in AST.
    /// \details Children for types that support them:
    ///          - KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION: If items exist, then variable is array and
    ///                                                               items are array dimensions sizes.
    ///          - KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION: 0 is left operand and 1 is right operand.
    ///          - KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION: 0 is operand.
    ///          - KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE: All items are expressions inside scope.
    ///          - KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL: All children are function arguments.
    ///          - KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR: All children are constructor arguments.
    ///          - KAN_RPL_EXPRESSION_NODE_TYPE_IF: 0 is condition, 1 is body scope and 2 is else body
    ///                                             scope (if present).
    ///          - KAN_RPL_EXPRESSION_NODE_TYPE_FOR: 0 is initializer, 1 is condition, 2 is incrementer and
    ///                                              3 is body scope.
    ///          - KAN_RPL_EXPRESSION_NODE_TYPE_WHILE: 0 is condition and 1 is body scope.
    ///          - KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE: 0 is condition and 1 is body scope.
    ///          - KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS: 0 is condition and 1 is alias replacement.
    ///          - KAN_RPL_EXPRESSION_NODE_TYPE_RETURN: 0 is return value.
    ///
    /// \meta reflection_dynamic_array_type = "struct kan_rpl_expression_node_t"
    struct kan_dynamic_array_t children;

    kan_interned_string_t source_name;
    uint32_t source_line;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_expression_node_init (struct kan_rpl_expression_node_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_expression_node_shutdown (struct kan_rpl_expression_node_t *instance);

enum kan_rpl_setting_type_t
{
    KAN_RPL_SETTING_TYPE_FLAG = 0u,
    KAN_RPL_SETTING_TYPE_INTEGER,
    KAN_RPL_SETTING_TYPE_FLOATING,
    KAN_RPL_SETTING_TYPE_STRING,
};

struct kan_rpl_setting_t
{
    kan_interned_string_t name;
    enum kan_rpl_setting_type_t type;

    union
    {
        /// \meta reflection_visibility_condition_field = "type"
        /// \meta reflection_visibility_condition_values = "(int64_t) KAN_RPL_SETTING_TYPE_FLAG"
        kan_bool_t flag;

        /// \meta reflection_visibility_condition_field = "type"
        /// \meta reflection_visibility_condition_values = "(int64_t) KAN_RPL_SETTING_TYPE_INTEGER"
        int64_t integer;

        /// \meta reflection_visibility_condition_field = "type"
        /// \meta reflection_visibility_condition_values = "(int64_t) KAN_RPL_SETTING_TYPE_FLOATING"
        double floating;

        /// \meta reflection_visibility_condition_field = "type"
        /// \meta reflection_visibility_condition_values = "(int64_t) KAN_RPL_SETTING_TYPE_STRING"
        kan_interned_string_t string;
    };

    /// \details Conditional expression if it is not KAN_RPL_EXPRESSION_NODE_TYPE_NOPE.
    struct kan_rpl_expression_node_t conditional;

    kan_interned_string_t source_name;
    uint32_t source_line;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_setting_init (struct kan_rpl_setting_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_setting_shutdown (struct kan_rpl_setting_t *instance);

struct kan_rpl_declaration_t
{
    kan_interned_string_t type_name;
    kan_interned_string_t name;

    /// \details Array size expressions if field is array.
    /// \meta reflection_dynamic_array_type = "struct kan_rpl_expression_node_t"
    struct kan_dynamic_array_t array_sizes;

    /// \details Conditional expression if it is not KAN_RPL_EXPRESSION_NODE_TYPE_NOPE.
    struct kan_rpl_expression_node_t conditional;

    /// \details Array of meta-tags on this declaration.
    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t meta;

    kan_interned_string_t source_name;
    uint32_t source_line;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_declaration_init (struct kan_rpl_declaration_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_declaration_shutdown (struct kan_rpl_declaration_t *instance);

struct kan_rpl_struct_t
{
    kan_interned_string_t name;

    /// \meta reflection_dynamic_array_type = "struct kan_rpl_declaration_t"
    struct kan_dynamic_array_t fields;

    /// \details Conditional expression if it is not KAN_RPL_EXPRESSION_NODE_TYPE_NOPE.
    struct kan_rpl_expression_node_t conditional;

    kan_interned_string_t source_name;
    uint32_t source_line;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_struct_init (struct kan_rpl_struct_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_struct_shutdown (struct kan_rpl_struct_t *instance);

enum kan_rpl_buffer_type_t
{
    KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE = 0u,
    KAN_RPL_BUFFER_TYPE_UNIFORM,
    KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE,
    KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE,
    KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM,
    KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE,

    /// \details Not really a buffer, but uses buffer semantics.
    KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT,

    /// \details Not really a buffer, but uses buffer semantics.
    KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT,
};

struct kan_rpl_buffer_t
{
    kan_interned_string_t name;
    enum kan_rpl_buffer_type_t type;

    /// \meta reflection_dynamic_array_type = "struct kan_rpl_declaration_t"
    struct kan_dynamic_array_t fields;

    /// \details Conditional expression if it is not KAN_RPL_EXPRESSION_NODE_TYPE_NOPE.
    struct kan_rpl_expression_node_t conditional;

    kan_interned_string_t source_name;
    uint32_t source_line;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_buffer_init (struct kan_rpl_buffer_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_buffer_shutdown (struct kan_rpl_buffer_t *instance);

enum kan_rpl_sampler_type_t
{
    KAN_RPL_SAMPLER_TYPE_2D = 0u,
};

struct kan_rpl_sampler_t
{
    kan_interned_string_t name;
    enum kan_rpl_sampler_type_t type;

    /// \meta reflection_dynamic_array_type = "struct kan_rpl_setting_t"
    struct kan_dynamic_array_t settings;

    /// \details Conditional expression if it is not KAN_RPL_EXPRESSION_NODE_TYPE_NOPE.
    struct kan_rpl_expression_node_t conditional;

    kan_interned_string_t source_name;
    uint32_t source_line;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_sampler_init (struct kan_rpl_sampler_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_sampler_shutdown (struct kan_rpl_sampler_t *instance);

struct kan_rpl_function_t
{
    kan_interned_string_t return_type_name;
    kan_interned_string_t name;

    /// \meta reflection_dynamic_array_type = "struct kan_rpl_declaration_t"
    struct kan_dynamic_array_t arguments;

    struct kan_rpl_expression_node_t body;

    /// \details Conditional expression if it is not KAN_RPL_EXPRESSION_NODE_TYPE_NOPE.
    struct kan_rpl_expression_node_t conditional;

    kan_interned_string_t source_name;
    uint32_t source_line;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_function_init (struct kan_rpl_function_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_function_shutdown (struct kan_rpl_function_t *instance);

enum kan_rpl_pipeline_type_t
{
    KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC = 0u,
};

struct kan_rpl_intermediate_t
{
    kan_interned_string_t log_name;

    /// \meta reflection_dynamic_array_type = "struct kan_rpl_option_t"
    struct kan_dynamic_array_t options;

    /// \meta reflection_dynamic_array_type = "struct kan_rpl_setting_t"
    struct kan_dynamic_array_t settings;

    /// \meta reflection_dynamic_array_type = "struct kan_rpl_struct_t"
    struct kan_dynamic_array_t structs;

    /// \meta reflection_dynamic_array_type = "struct kan_rpl_buffer_t"
    struct kan_dynamic_array_t buffers;

    /// \meta reflection_dynamic_array_type = "struct kan_rpl_sampler_t"
    struct kan_dynamic_array_t samplers;

    /// \meta reflection_dynamic_array_type = "struct kan_rpl_function_t"
    struct kan_dynamic_array_t functions;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_intermediate_init (struct kan_rpl_intermediate_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_intermediate_shutdown (struct kan_rpl_intermediate_t *instance);

enum kan_rpl_pipeline_stage_t
{
    KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX = 0u,
    KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT,
};

typedef uint64_t kan_rpl_parser_t;

RENDER_PIPELINE_LANGUAGE_API kan_rpl_parser_t kan_rpl_parser_create (kan_interned_string_t log_name);

RENDER_PIPELINE_LANGUAGE_API kan_bool_t kan_rpl_parser_add_source (kan_rpl_parser_t parser,
                                                                   const char *source,
                                                                   kan_interned_string_t log_name);

RENDER_PIPELINE_LANGUAGE_API kan_bool_t kan_rpl_parser_build_intermediate (kan_rpl_parser_t parser,
                                                                           struct kan_rpl_intermediate_t *output);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_parser_destroy (kan_rpl_parser_t parser);

KAN_C_HEADER_END