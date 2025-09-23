#pragma once

#include <render_pipeline_language_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/reflection/markup.h>

/// \file
/// \brief Contains declarations for parse step of render pipeline language and intermediate format declaration.

KAN_C_HEADER_BEGIN

/// \brief Supported option scopes.
enum kan_rpl_option_scope_t
{
    KAN_RPL_OPTION_SCOPE_GLOBAL = 0u,
    KAN_RPL_OPTION_SCOPE_INSTANCE,
};

/// \brief Supported option types.
enum kan_rpl_option_type_t
{
    KAN_RPL_OPTION_TYPE_FLAG = 0u,
    KAN_RPL_OPTION_TYPE_UINT,
    KAN_RPL_OPTION_TYPE_SINT,
    KAN_RPL_OPTION_TYPE_FLOAT,
    KAN_RPL_OPTION_TYPE_ENUM,
};

/// \brief Enum option serializable data.
/// \details Supported enum values are stored among other serializable strings in
///          `kan_rpl_intermediate_t::string_lists_storage`.
struct kan_rpl_option_enum_t
{
    kan_instance_size_t list_size;
    kan_instance_size_t list_index;
};

/// \brief Defines intermediate format to store parsed option.
struct kan_rpl_option_t
{
    kan_interned_string_t name;
    enum kan_rpl_option_scope_t scope;
    enum kan_rpl_option_type_t type;

    union
    {
        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_OPTION_TYPE_FLAG)
        bool flag_default_value;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_OPTION_TYPE_UINT)
        kan_instance_size_t uint_default_value;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_OPTION_TYPE_SINT)
        kan_instance_size_t sint_default_value;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_OPTION_TYPE_FLOAT)
        kan_floating_t float_default_value;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_OPTION_TYPE_ENUM)
        struct kan_rpl_option_enum_t enum_values;
    };
};

/// \brief Enumerates supported binary operations.
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

/// \brief Enumerates supported unary operations.
enum kan_rpl_unary_operation_t
{
    KAN_RPL_UNARY_OPERATION_NEGATE = 0u,
    KAN_RPL_UNARY_OPERATION_NOT,
    KAN_RPL_UNARY_OPERATION_BITWISE_NOT,
};

/// \brief Enumerates supported expression types.
enum kan_rpl_expression_type_t
{
    KAN_RPL_EXPRESSION_NODE_TYPE_NOPE = 0u,
    KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER,
    KAN_RPL_EXPRESSION_NODE_TYPE_BOOLEAN_LITERAL,
    KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL,
    KAN_RPL_EXPRESSION_NODE_TYPE_UNSIGNED_LITERAL,
    KAN_RPL_EXPRESSION_NODE_TYPE_SIGNED_LITERAL,
    KAN_RPL_EXPRESSION_NODE_TYPE_STRING_LITERAL,
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

/// \brief Expression index used to specify absence of expression.
#define KAN_RPL_EXPRESSION_INDEX_NONE UINT32_MAX

/// \brief Additional expression data for variable declarations.
struct kan_rpl_variable_declaration_data_t
{
    kan_interned_string_t type_name;
    kan_interned_string_t variable_name;
    kan_instance_size_t array_size_expression_list_size;
    kan_instance_size_t array_size_expression_list_index;
};

/// \brief Additional expression data for binary operations.
struct kan_rpl_binary_operation_data_t
{
    enum kan_rpl_binary_operation_t operation;
    kan_instance_size_t left_operand_index;
    kan_instance_size_t right_operand_index;
};

/// \brief Additional expression data for unary operations.
struct kan_rpl_unary_operation_data_t
{
    enum kan_rpl_unary_operation_t operation;
    kan_instance_size_t operand_index;
};

/// \brief Additional expression data for scopes.
struct kan_rpl_scope_data_t
{
    kan_instance_size_t statement_list_size;
    kan_instance_size_t statement_list_index;
};

/// \brief Additional expression data for function call.
struct kan_rpl_function_call_data_t
{
    kan_interned_string_t name;
    kan_instance_size_t argument_list_size;
    kan_instance_size_t argument_list_index;
};

/// \brief Additional expression data for constructor call.
struct kan_rpl_constructor_data_t
{
    kan_interned_string_t type_name;
    kan_instance_size_t argument_list_size;
    kan_instance_size_t argument_list_index;
};

/// \brief Additional expression data for if construction.
struct kan_rpl_if_data_t
{
    kan_instance_size_t condition_index;
    kan_instance_size_t true_index;
    kan_instance_size_t false_index;
};

/// \brief Additional expression data for for construction.
struct kan_rpl_for_data_t
{
    kan_instance_size_t init_index;
    kan_instance_size_t condition_index;
    kan_instance_size_t step_index;
    kan_instance_size_t body_index;
};

/// \brief Additional expression data for while construction.
struct kan_rpl_while_data_t
{
    kan_instance_size_t condition_index;
    kan_instance_size_t body_index;
};

/// \brief Additional expression data for conditional scope.
struct kan_rpl_conditional_scope_data_t
{
    kan_instance_size_t condition_index;
    kan_instance_size_t body_index;
};

/// \brief Additional expression data for conditional alias.
struct kan_rpl_conditional_alias_data_t
{
    kan_instance_size_t condition_index;
    kan_interned_string_t name;
    kan_instance_size_t expression_index;
};

/// \brief Defines structure that holds one expression data.
struct kan_rpl_expression_t
{
    enum kan_rpl_expression_type_t type;
    union
    {
        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER)
        kan_interned_string_t identifier;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_BOOLEAN_LITERAL)
        bool boolean_literal;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL)
        kan_floating_t floating_literal;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_UNSIGNED_LITERAL)
        kan_instance_size_t unsigned_literal;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_SIGNED_LITERAL)
        kan_instance_offset_t signed_literal;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_STRING_LITERAL)
        kan_interned_string_t string_literal;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION)
        struct kan_rpl_variable_declaration_data_t variable_declaration;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION)
        struct kan_rpl_binary_operation_data_t binary_operation;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION)
        struct kan_rpl_unary_operation_data_t unary_operation;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE)
        struct kan_rpl_scope_data_t scope;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL)
        struct kan_rpl_function_call_data_t function_call;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR)
        struct kan_rpl_constructor_data_t constructor;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_IF)
        struct kan_rpl_if_data_t if_;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_FOR)
        struct kan_rpl_for_data_t for_;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_WHILE)
        struct kan_rpl_while_data_t while_;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE)
        struct kan_rpl_conditional_scope_data_t conditional_scope;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS)
        struct kan_rpl_conditional_alias_data_t conditional_alias;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_EXPRESSION_NODE_TYPE_RETURN)
        kan_instance_size_t return_index;
    };

    kan_interned_string_t source_name;
    kan_instance_size_t source_line;
};

/// \brief Defines structure that holds one constant data.
struct kan_rpl_constant_t
{
    kan_interned_string_t name;

    kan_instance_size_t expression_index;

    kan_instance_size_t conditional_index;

    kan_interned_string_t source_name;
    kan_instance_size_t source_line;
};

#define KAN_RPL_SETTING_BLOCK_NONE UINT32_MAX

/// \brief Defines structure that holds one setting data.
struct kan_rpl_setting_t
{
    kan_interned_string_t name;
    kan_instance_size_t block;

    kan_instance_size_t expression_index;

    kan_instance_size_t conditional_index;

    kan_interned_string_t source_name;
    kan_instance_size_t source_line;
};

/// \brief Defines structure that holds one struct field declaration.
struct kan_rpl_declaration_t
{
    kan_interned_string_t type_name;
    kan_interned_string_t name;

    /// \brief If true, field is a runtime sized array and other array size expressions should be ignored.
    bool array_size_runtime;

    kan_instance_size_t array_size_expression_list_size;

    kan_instance_size_t array_size_expression_list_index;

    kan_instance_size_t conditional_index;

    kan_instance_size_t meta_list_size;
    kan_instance_size_t meta_list_index;

    kan_interned_string_t source_name;
    kan_instance_size_t source_line;
};

/// \brief Defines structure that holds structure data.
struct kan_rpl_struct_t
{
    kan_interned_string_t name;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_declaration_t)
    struct kan_dynamic_array_t fields;

    kan_instance_size_t conditional_index;

    kan_interned_string_t source_name;
    kan_instance_size_t source_line;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_struct_init (struct kan_rpl_struct_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_struct_shutdown (struct kan_rpl_struct_t *instance);

/// \brief Defines set types in which buffer and sampler bindings are contained.
/// \details Set index in generated code is equal to the value of this enumeration.
///          There can be no more that 4 sets as of autumn 2024 as it is minimum guaranteed count on modern GPUs.
///          Having more would make some old hardware unusable for us.
enum kan_rpl_set_t
{
    /// \brief Set for data that is bound once during pass and
    ///        shared across everything that is rendered inside the pass.
    KAN_RPL_SET_PASS = 0u,

    /// \brief Set for data that is bound on per-material basis.
    KAN_RPL_SET_MATERIAL = 1u,

    /// \brief Set for data that is bound on per-object basis.
    KAN_RPL_SET_OBJECT = 2u,

    /// \brief Set for the runtime data that might be shared between several objects, but is not on the material level.
    /// \details For example, skeleton of even buffer with several skeletons might be shared across several objects
    ///          in order to make instancing possible for these objects.
    KAN_RPL_SET_SHARED = 3u,
};

/// \brief Enumerates supported container types.
enum kan_rpl_container_type_t
{
    KAN_RPL_CONTAINER_TYPE_VERTEX_ATTRIBUTE = 0u,
    KAN_RPL_CONTAINER_TYPE_INSTANCED_ATTRIBUTE,
    KAN_RPL_CONTAINER_TYPE_STATE,
    KAN_RPL_CONTAINER_TYPE_COLOR_OUTPUT,
};

/// \brief Enumerates known input attribute pack types.
enum kan_rpl_input_pack_class_t
{
    /// \brief Special values that indicates that packing was not specified and original format should be used.
    KAN_RPL_INPUT_PACK_CLASS_DEFAULT = 0u,

    KAN_RPL_INPUT_PACK_CLASS_FLOAT,
    KAN_RPL_INPUT_PACK_CLASS_UNORM,
    KAN_RPL_INPUT_PACK_CLASS_SNORM,
    KAN_RPL_INPUT_PACK_CLASS_UINT,
    KAN_RPL_INPUT_PACK_CLASS_SINT,
};

/// \brief Defines structure that holds container field data.
struct kan_rpl_container_field_t
{
    kan_interned_string_t type_name;
    kan_interned_string_t name;

    enum kan_rpl_input_pack_class_t pack_class;
    union
    {
        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (pack_class)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_INPUT_PACK_CLASS_FLOAT)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_INPUT_PACK_CLASS_UNORM)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_INPUT_PACK_CLASS_SNORM)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_INPUT_PACK_CLASS_UINT)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_INPUT_PACK_CLASS_SINT)
        kan_instance_size_t pack_class_bits;
    };

    kan_instance_size_t conditional_index;

    kan_instance_size_t meta_list_size;
    kan_instance_size_t meta_list_index;

    kan_interned_string_t source_name;
    kan_instance_size_t source_line;
};

/// \brief Defines structure that holds container data.
struct kan_rpl_container_t
{
    kan_interned_string_t name;
    enum kan_rpl_container_type_t type;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_container_field_t)
    struct kan_dynamic_array_t fields;

    kan_instance_size_t conditional_index;

    kan_interned_string_t source_name;
    kan_instance_size_t source_line;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_container_init (struct kan_rpl_container_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_container_shutdown (struct kan_rpl_container_t *instance);

/// \brief Enumerates supported buffer types.
enum kan_rpl_buffer_type_t
{
    KAN_RPL_BUFFER_TYPE_UNIFORM = 0u,
    KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE,

    /// \brief Push constant layouts are expressed through buffer syntax.
    KAN_RPL_BUFFER_TYPE_PUSH_CONSTANT,
};

/// \brief Defines structure that holds buffer data.
struct kan_rpl_buffer_t
{
    kan_interned_string_t name;
    enum kan_rpl_set_t set;
    enum kan_rpl_buffer_type_t type;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_declaration_t)
    struct kan_dynamic_array_t fields;

    kan_instance_size_t conditional_index;

    kan_interned_string_t source_name;
    kan_instance_size_t source_line;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_buffer_init (struct kan_rpl_buffer_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_buffer_shutdown (struct kan_rpl_buffer_t *instance);

/// \brief Defines structure that holds sampler declaration.
struct kan_rpl_sampler_t
{
    kan_interned_string_t name;
    enum kan_rpl_set_t set;

    kan_instance_size_t conditional_index;

    kan_interned_string_t source_name;
    kan_instance_size_t source_line;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_sampler_init (struct kan_rpl_sampler_t *instance);

/// \brief Enumerates supported image types.
enum kan_rpl_image_type_t
{
    KAN_RPL_IMAGE_TYPE_COLOR_2D = 0u,
    KAN_RPL_IMAGE_TYPE_COLOR_3D,
    KAN_RPL_IMAGE_TYPE_COLOR_CUBE,
    KAN_RPL_IMAGE_TYPE_COLOR_2D_ARRAY,
    KAN_RPL_IMAGE_TYPE_DEPTH_2D,
    KAN_RPL_IMAGE_TYPE_DEPTH_3D,
    KAN_RPL_IMAGE_TYPE_DEPTH_CUBE,
    KAN_RPL_IMAGE_TYPE_DEPTH_2D_ARRAY,

    /// \brief Interval value used as counter for image types.
    KAN_RPL_IMAGE_TYPE_COUNT,
};

/// \brief Defines structure that holds image or image array declaration.
struct kan_rpl_image_t
{
    kan_interned_string_t name;
    enum kan_rpl_set_t set;
    enum kan_rpl_image_type_t type;

    kan_instance_size_t array_size_index;

    kan_instance_size_t conditional_index;

    kan_interned_string_t source_name;
    kan_instance_size_t source_line;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_image_init (struct kan_rpl_image_t *instance);

/// \brief Enumerates access classes for function arguments.
enum kan_rpl_access_class_t
{
    KAN_RPL_ACCESS_CLASS_READ_ONLY = 0u,
    KAN_RPL_ACCESS_CLASS_WRITE_ONLY,
    KAN_RPL_ACCESS_CLASS_READ_WRITE,
};

/// \brief Defines structure that holds one function argument declaration.
/// \details Different from `kan_rpl_declaration_t` as function arguments have some differences from structure fields.
struct kan_rpl_function_argument_t
{
    kan_interned_string_t type_name;
    kan_interned_string_t name;
    enum kan_rpl_access_class_t access;

    kan_instance_size_t array_size_expression_list_size;

    kan_instance_size_t array_size_expression_list_index;

    kan_instance_size_t conditional_index;

    kan_interned_string_t source_name;
    kan_instance_size_t source_line;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_function_argument_init (struct kan_rpl_function_argument_t *instance);

/// \brief Defines structure that holds function data.
struct kan_rpl_function_t
{
    kan_interned_string_t return_type_name;
    kan_interned_string_t name;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_function_argument_t)
    struct kan_dynamic_array_t arguments;

    kan_instance_size_t body_index;

    kan_instance_size_t conditional_index;

    kan_interned_string_t source_name;
    kan_instance_size_t source_line;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_function_init (struct kan_rpl_function_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_function_shutdown (struct kan_rpl_function_t *instance);

/// \brief Enumerates supported pipeline types.
enum kan_rpl_pipeline_type_t
{
    KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC = 0u,
};

/// \brief Defines format for storing parsed render pipeline language data.
struct kan_rpl_intermediate_t
{
    /// \brief Name for logging, so user can understand from what this data was generated.
    kan_interned_string_t log_name;

    /// \brief Array of parsed options.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_option_t)
    struct kan_dynamic_array_t options;

    /// \brief Array of parsed constants.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_constant_t)
    struct kan_dynamic_array_t constants;

    /// \brief Array of parsed settings.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_setting_t)
    struct kan_dynamic_array_t settings;

    /// \brief Array of parsed structs.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_struct_t)
    struct kan_dynamic_array_t structs;

    /// \brief Array of parsed containers.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_container_t)
    struct kan_dynamic_array_t containers;

    /// \brief Array of parsed buffers.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_buffer_t)
    struct kan_dynamic_array_t buffers;

    /// \brief Array of parsed samplers.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_sampler_t)
    struct kan_dynamic_array_t samplers;

    /// \brief Array of parsed images.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_image_t)
    struct kan_dynamic_array_t images;

    /// \brief Array of parsed functions.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_function_t)
    struct kan_dynamic_array_t functions;

    /// \brief Array of parsed expressions.
    /// \details Expression are stored in special array for better performance
    ///          through better utilization of memory caching techniques.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_expression_t)
    struct kan_dynamic_array_t expression_storage;

    /// \brief Array that is used to store expression indices for expression lists.
    /// \details For the same reason as expressions, lists of expression indices are stored in one continuous array.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_instance_size_t)
    struct kan_dynamic_array_t expression_lists_storage;

    /// \brief Array that is used to store all strings, including enum values and meta tags.
    /// \details For the same reason as expressions, lists of strings are stored in one continuous array.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t string_lists_storage;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_intermediate_init (struct kan_rpl_intermediate_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_intermediate_shutdown (struct kan_rpl_intermediate_t *instance);

enum kan_rpl_pipeline_stage_t
{
    KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX = 0u,
    KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT,
};

KAN_HANDLE_DEFINE (kan_rpl_parser_t);

/// \brief Creates new instance of render pipeline language parser.
RENDER_PIPELINE_LANGUAGE_API kan_rpl_parser_t kan_rpl_parser_create (kan_interned_string_t log_name);

/// \brief Parses given null-terminated string as source and appends it to previous parsed data if any.
RENDER_PIPELINE_LANGUAGE_API bool kan_rpl_parser_add_source (kan_rpl_parser_t parser,
                                                             const char *source,
                                                             kan_interned_string_t log_name);

/// \brief Builds intermediate structure from parsed data.
RENDER_PIPELINE_LANGUAGE_API bool kan_rpl_parser_build_intermediate (kan_rpl_parser_t parser,
                                                                     struct kan_rpl_intermediate_t *output);

/// \brief Destroys given render pipeline language parser.
RENDER_PIPELINE_LANGUAGE_API void kan_rpl_parser_destroy (kan_rpl_parser_t parser);

KAN_C_HEADER_END
