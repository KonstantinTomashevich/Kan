#include <stddef.h>
#include <string.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/render_pipeline_language/parser.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (rpl_parser);

struct parser_option_enum_value_t
{
    struct parser_option_enum_value_t *next;
    kan_interned_string_t name;
};

struct parser_option_t
{
    struct parser_option_t *next;
    kan_interned_string_t name;
    enum kan_rpl_option_scope_t scope;
    enum kan_rpl_option_type_t type;

    union
    {
        kan_bool_t flag_default_value;
        kan_rpl_unsigned_int_literal_t uint_default_value;
        kan_rpl_signed_int_literal_t sint_default_value;
        float float_default_value;
        struct parser_option_enum_value_t *first_enum_value;
    };
};

struct parser_expression_list_item_t
{
    struct parser_expression_list_item_t *next;
    struct parser_expression_tree_node_t *expression;
};

struct parser_declaration_data_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    kan_bool_t array_size_runtime;
    struct parser_expression_list_item_t *array_size_list;
};

struct parser_expression_binary_operation_t
{
    enum kan_rpl_binary_operation_t binary_operation;
    struct parser_expression_tree_node_t *left_operand_expression;
    struct parser_expression_tree_node_t *right_operand_expression;
};

struct parser_expression_unary_operation_t
{
    enum kan_rpl_unary_operation_t unary_operation;
    struct parser_expression_tree_node_t *operand_expression;
};

struct parser_expression_function_call_t
{
    kan_interned_string_t function_name;
    struct parser_expression_list_item_t *arguments;
};

struct parser_expression_constructor_t
{
    kan_interned_string_t constructor_type_name;
    struct parser_expression_list_item_t *arguments;
};

struct parser_expression_if_t
{
    struct parser_expression_tree_node_t *condition_expression;
    struct parser_expression_tree_node_t *true_expression;
    struct parser_expression_tree_node_t *false_expression;
};

struct parser_expression_for_t
{
    struct parser_expression_tree_node_t *init_expression;
    struct parser_expression_tree_node_t *condition_expression;
    struct parser_expression_tree_node_t *step_expression;
    struct parser_expression_tree_node_t *body_expression;
};

struct parser_expression_while_t
{
    struct parser_expression_tree_node_t *condition_expression;
    struct parser_expression_tree_node_t *body_expression;
};

struct parser_expression_conditional_scope_t
{
    struct parser_expression_tree_node_t *condition_expression;
    struct parser_expression_tree_node_t *body_expression;
};

struct parser_expression_conditional_alias_t
{
    struct parser_expression_tree_node_t *condition_expression;
    kan_interned_string_t identifier;
    struct parser_expression_tree_node_t *body_expression;
};

struct parser_expression_tree_node_t
{
    struct parser_expression_tree_node_t *parent_expression;
    enum kan_rpl_expression_type_t type;

    union
    {
        kan_interned_string_t identifier;

        kan_rpl_unsigned_int_literal_t signed_literal;

        kan_rpl_signed_int_literal_t unsigned_literal;

        float floating_literal;

        struct parser_declaration_data_t variable_declaration;

        struct parser_expression_binary_operation_t binary_operation;

        struct parser_expression_unary_operation_t unary_operation;

        struct parser_expression_list_item_t *scope_expressions_list;

        struct parser_expression_function_call_t function_call;

        struct parser_expression_constructor_t constructor;

        struct parser_expression_if_t if_;

        struct parser_expression_for_t for_;

        struct parser_expression_while_t while_;

        struct parser_expression_conditional_scope_t conditional_scope;

        struct parser_expression_conditional_alias_t conditional_alias;

        struct parser_expression_tree_node_t *return_expression;
    };

    kan_interned_string_t source_log_name;
    kan_rpl_size_t source_line;
};

struct parser_setting_data_t
{
    kan_interned_string_t name;
    kan_rpl_size_t block;
    enum kan_rpl_setting_type_t type;

    union
    {
        kan_bool_t flag;
        kan_rpl_signed_int_literal_t integer;
        float floating;
        kan_interned_string_t string;
    };

    struct parser_expression_tree_node_t *conditional;
    kan_interned_string_t source_log_name;
    kan_rpl_size_t source_line;
};

struct parser_setting_t
{
    struct parser_setting_t *next;
    struct parser_setting_data_t setting;
};

struct parser_declaration_meta_item_t
{
    struct parser_declaration_meta_item_t *next;
    kan_interned_string_t meta;
};

struct parser_declaration_t
{
    struct parser_declaration_t *next;
    struct parser_declaration_data_t declaration;
    struct parser_declaration_meta_item_t *first_meta;
    struct parser_expression_tree_node_t *conditional;
    kan_interned_string_t source_log_name;
    kan_rpl_size_t source_line;
};

struct parser_struct_t
{
    struct parser_struct_t *next;
    kan_interned_string_t name;
    struct parser_declaration_t *first_declaration;

    struct parser_expression_tree_node_t *conditional;
    kan_interned_string_t source_log_name;
    kan_rpl_size_t source_line;
};

struct parser_container_field_t
{
    struct parser_container_field_t *next;
    struct parser_declaration_data_t declaration;
    enum kan_rpl_input_pack_class_t pack_class;
    kan_rpl_size_t pack_class_bits;
    struct parser_declaration_meta_item_t *first_meta;
    struct parser_expression_tree_node_t *conditional;
    kan_interned_string_t source_log_name;
    kan_rpl_size_t source_line;
};

struct parser_container_t
{
    struct parser_container_t *next;
    kan_interned_string_t name;
    enum kan_rpl_container_type_t type;
    struct parser_container_field_t *first_field;

    struct parser_expression_tree_node_t *conditional;
    kan_interned_string_t source_log_name;
    kan_rpl_size_t source_line;
};

struct parser_buffer_t
{
    struct parser_buffer_t *next;
    kan_interned_string_t name;
    enum kan_rpl_set_t set;
    enum kan_rpl_buffer_type_t type;
    struct parser_declaration_t *first_declaration;

    struct parser_expression_tree_node_t *conditional;
    kan_interned_string_t source_log_name;
    kan_rpl_size_t source_line;
};

struct parser_setting_list_item_t
{
    struct parser_setting_list_item_t *next;
    struct parser_setting_data_t setting;
};

struct parser_sampler_t
{
    struct parser_sampler_t *next;
    kan_interned_string_t name;
    enum kan_rpl_set_t set;

    struct parser_expression_tree_node_t *conditional;
    kan_interned_string_t source_log_name;
    kan_rpl_size_t source_line;
};

struct parser_image_t
{
    struct parser_image_t *next;
    kan_interned_string_t name;
    enum kan_rpl_set_t set;
    enum kan_rpl_image_type_t type;

    struct parser_expression_tree_node_t *array_size;
    struct parser_expression_tree_node_t *conditional;

    kan_interned_string_t source_log_name;
    kan_rpl_size_t source_line;
};

struct parser_function_argument_t
{
    struct parser_function_argument_t *next;
    struct parser_declaration_data_t declaration;
    enum kan_rpl_access_class_t access;
    struct parser_expression_tree_node_t *conditional;
    kan_interned_string_t source_log_name;
    kan_rpl_size_t source_line;
};

struct parser_function_t
{
    struct parser_function_t *next;
    kan_interned_string_t return_type_name;
    kan_interned_string_t name;
    struct parser_function_argument_t *first_argument;
    struct parser_expression_tree_node_t *body_expression;

    struct parser_expression_tree_node_t *conditional;
    kan_interned_string_t source_log_name;
    kan_rpl_size_t source_line;
};

struct parser_processing_data_t
{
    struct parser_option_t *first_option;
    struct parser_option_t *last_option;
    kan_instance_size_t option_count;

    struct parser_setting_t *first_setting;
    struct parser_setting_t *last_setting;
    kan_instance_size_t setting_count;

    struct parser_struct_t *first_struct;
    struct parser_struct_t *last_struct;
    kan_instance_size_t struct_count;

    struct parser_container_t *first_container;
    struct parser_container_t *last_container;
    kan_instance_size_t container_count;

    struct parser_buffer_t *first_buffer;
    struct parser_buffer_t *last_buffer;
    kan_instance_size_t buffer_count;

    struct parser_sampler_t *first_sampler;
    struct parser_sampler_t *last_sampler;
    kan_instance_size_t sampler_count;

    struct parser_image_t *first_image;
    struct parser_image_t *last_image;
    kan_instance_size_t image_count;

    struct parser_function_t *first_function;
    struct parser_function_t *last_function;
    kan_instance_size_t function_count;
};

struct rpl_parser_t
{
    kan_interned_string_t log_name;
    struct kan_stack_group_allocator_t allocator;
    struct parser_processing_data_t processing_data;
};

struct dynamic_parser_state_t
{
    kan_interned_string_t source_log_name;
    struct parser_expression_tree_node_t *detached_conditional;

    const char *limit;
    const char *cursor;
    const char *marker;
    const char *token;

    kan_instance_size_t cursor_line;
    kan_instance_size_t cursor_symbol;
    kan_instance_size_t marker_line;
    kan_instance_size_t marker_symbol;

    const char *saved;
    kan_instance_size_t saved_line;
    kan_instance_size_t saved_symbol;

    /*!stags:re2c format = 'const char *@@;';*/
};

struct expression_parse_state_t
{
    kan_bool_t expecting_operand;
    struct parser_expression_tree_node_t *current_node;
};

static kan_bool_t statics_initialized = KAN_FALSE;
static kan_allocation_group_t rpl_allocation_group;
static kan_allocation_group_t rpl_parser_allocation_group;
static kan_allocation_group_t rpl_intermediate_allocation_group;

static kan_instance_size_t unary_operation_priority = 11u;

static kan_instance_size_t binary_operation_priority[] = {
    /* KAN_RPL_BINARY_OPERATION_FIELD_ACCESS */ 12u,
    /* KAN_RPL_BINARY_OPERATION_ARRAY_ACCESS */ 12u,
    /* KAN_RPL_BINARY_OPERATION_ADD */ 9u,
    /* KAN_RPL_BINARY_OPERATION_SUBTRACT */ 9u,
    /* KAN_RPL_BINARY_OPERATION_MULTIPLY */ 10u,
    /* KAN_RPL_BINARY_OPERATION_DIVIDE */ 10u,
    /* KAN_RPL_BINARY_OPERATION_MODULUS */ 10u,
    /* KAN_RPL_BINARY_OPERATION_ASSIGN */ 0u,
    /* KAN_RPL_BINARY_OPERATION_AND */ 2u,
    /* KAN_RPL_BINARY_OPERATION_OR */ 1u,
    /* KAN_RPL_BINARY_OPERATION_EQUAL */ 6u,
    /* KAN_RPL_BINARY_OPERATION_NOT_EQUAL */ 6u,
    /* KAN_RPL_BINARY_OPERATION_LESS */ 7u,
    /* KAN_RPL_BINARY_OPERATION_GREATER */ 7u,
    /* KAN_RPL_BINARY_OPERATION_LESS_OR_EQUAL */ 7u,
    /* KAN_RPL_BINARY_OPERATION_GREATER_OR_EQUAL */ 7u,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_AND */ 5u,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_OR */ 3u,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_XOR */ 4u,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_LSHIFT */ 8u,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_RSHIFT */ 8u,
};

enum binary_operation_direction_t
{
    BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT = 0u,
    BINARY_OPERATION_DIRECTION_RIGHT_TO_LEFT,
};

static kan_instance_size_t binary_operation_direction[] = {
    /* KAN_RPL_BINARY_OPERATION_FIELD_ACCESS */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_ARRAY_ACCESS */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_ADD */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_SUBTRACT */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_MULTIPLY */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_DIVIDE */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_MODULUS */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_ASSIGN */ BINARY_OPERATION_DIRECTION_RIGHT_TO_LEFT,
    /* KAN_RPL_BINARY_OPERATION_AND */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_OR */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_EQUAL */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_NOT_EQUAL */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_LESS */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_GREATER */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_LESS_OR_EQUAL */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_GREATER_OR_EQUAL */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_AND */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_OR */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_XOR */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_LSHIFT */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_RSHIFT */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
};

static kan_interned_string_t interned_void;

static inline void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        rpl_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "render_pipeline_language");
        rpl_parser_allocation_group = kan_allocation_group_get_child (rpl_allocation_group, "parser");
        rpl_intermediate_allocation_group = kan_allocation_group_get_child (rpl_allocation_group, "intermediate");

        interned_void = kan_string_intern ("void");
        statics_initialized = KAN_TRUE;
    }
}

static inline struct parser_expression_tree_node_t *parser_expression_tree_node_new (
    struct rpl_parser_t *parser,
    enum kan_rpl_expression_type_t type,
    kan_interned_string_t source_log_name,
    kan_rpl_size_t source_line)
{
    struct parser_expression_tree_node_t *node =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_expression_tree_node_t);

    node->parent_expression = NULL;
    node->type = type;
    node->source_log_name = source_log_name;
    node->source_line = source_line;

    switch (type)
    {
    case KAN_RPL_EXPRESSION_NODE_TYPE_NOPE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_BREAK:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONTINUE:
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER:
        node->identifier = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL:
        node->floating_literal = 0.0;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_SIGNED_LITERAL:
        node->signed_literal = 0;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_UNSIGNED_LITERAL:
        node->unsigned_literal = 0u;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION:
        node->variable_declaration.type = NULL;
        node->variable_declaration.name = NULL;
        node->variable_declaration.array_size_runtime = KAN_FALSE;
        node->variable_declaration.array_size_list = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION:
        node->binary_operation.binary_operation = KAN_RPL_BINARY_OPERATION_ADD;
        node->binary_operation.left_operand_expression = NULL;
        node->binary_operation.right_operand_expression = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION:
        node->unary_operation.unary_operation = KAN_RPL_UNARY_OPERATION_NEGATE;
        node->unary_operation.operand_expression = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE:
        node->scope_expressions_list = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL:
        node->function_call.function_name = NULL;
        node->function_call.arguments = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR:
        node->constructor.constructor_type_name = NULL;
        node->constructor.arguments = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_IF:
        node->if_.condition_expression = NULL;
        node->if_.true_expression = NULL;
        node->if_.false_expression = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FOR:
        node->for_.init_expression = NULL;
        node->for_.condition_expression = NULL;
        node->for_.step_expression = NULL;
        node->for_.body_expression = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_WHILE:
        node->while_.condition_expression = NULL;
        node->while_.body_expression = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE:
        node->conditional_scope.condition_expression = NULL;
        node->conditional_scope.body_expression = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS:
        node->conditional_alias.condition_expression = NULL;
        node->conditional_alias.identifier = NULL;
        node->conditional_alias.body_expression = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_RETURN:
        node->return_expression = NULL;
        break;
    }

    return node;
}

static inline struct parser_declaration_t *parser_declaration_new (struct rpl_parser_t *parser,
                                                                   kan_interned_string_t source_log_name,
                                                                   kan_rpl_size_t source_line)
{
    struct parser_declaration_t *declaration =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_declaration_t);
    declaration->next = NULL;
    declaration->declaration.name = NULL;
    declaration->declaration.type = NULL;
    declaration->declaration.array_size_runtime = KAN_FALSE;
    declaration->declaration.array_size_list = NULL;
    declaration->first_meta = NULL;
    declaration->conditional = NULL;
    declaration->source_log_name = source_log_name;
    declaration->source_line = source_line;
    return declaration;
}

static inline struct parser_struct_t *parser_struct_new (struct rpl_parser_t *parser,
                                                         kan_interned_string_t name,
                                                         kan_interned_string_t source_log_name,
                                                         kan_rpl_size_t source_line)
{
    struct parser_struct_t *instance =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_struct_t);
    instance->next = NULL;
    instance->name = name;
    instance->first_declaration = NULL;
    instance->conditional = NULL;
    instance->source_log_name = source_log_name;
    instance->source_line = source_line;
    return instance;
}

static inline struct parser_container_field_t *parser_container_field_new (struct rpl_parser_t *parser,
                                                                           kan_interned_string_t source_log_name,
                                                                           kan_rpl_size_t source_line)
{
    struct parser_container_field_t *field =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_container_field_t);
    field->next = NULL;
    field->declaration.name = NULL;
    field->declaration.type = NULL;
    field->declaration.array_size_runtime = KAN_FALSE;
    field->declaration.array_size_list = NULL;
    field->pack_class = KAN_RPL_INPUT_PACK_CLASS_DEFAULT;
    field->pack_class_bits = 0u;
    field->first_meta = NULL;
    field->conditional = NULL;
    field->source_log_name = source_log_name;
    field->source_line = source_line;
    return field;
}

static inline struct parser_container_t *parser_container_new (struct rpl_parser_t *parser,
                                                               kan_interned_string_t name,
                                                               kan_interned_string_t source_log_name,
                                                               kan_rpl_size_t source_line)
{
    struct parser_container_t *instance =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_container_t);
    instance->next = NULL;
    instance->name = name;
    instance->first_field = NULL;
    instance->conditional = NULL;
    instance->source_log_name = source_log_name;
    instance->source_line = source_line;
    return instance;
}

static inline struct parser_buffer_t *parser_buffer_new (struct rpl_parser_t *parser,
                                                         kan_interned_string_t name,
                                                         enum kan_rpl_set_t set,
                                                         kan_interned_string_t source_log_name,
                                                         kan_rpl_size_t source_line)
{
    struct parser_buffer_t *instance =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_buffer_t);
    instance->next = NULL;
    instance->name = name;
    instance->set = set;
    instance->first_declaration = NULL;
    instance->conditional = NULL;
    instance->source_log_name = source_log_name;
    instance->source_line = source_line;
    return instance;
}

static inline struct parser_sampler_t *parser_sampler_new (struct rpl_parser_t *parser,
                                                           kan_interned_string_t name,
                                                           enum kan_rpl_set_t set,
                                                           kan_interned_string_t source_log_name,
                                                           kan_rpl_size_t source_line)
{
    struct parser_sampler_t *instance =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_sampler_t);
    instance->next = NULL;
    instance->name = name;
    instance->set = set;
    instance->conditional = NULL;
    instance->source_log_name = source_log_name;
    instance->source_line = source_line;
    return instance;
}

static inline struct parser_image_t *parser_image_new (struct rpl_parser_t *parser,
                                                       enum kan_rpl_set_t set,
                                                       enum kan_rpl_image_type_t type,
                                                       kan_interned_string_t source_log_name,
                                                       kan_rpl_size_t source_line)
{
    struct parser_image_t *instance =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_image_t);
    instance->next = NULL;
    instance->name = NULL; // No name yet.
    instance->set = set;
    instance->type = KAN_RPL_IMAGE_TYPE_COLOR_2D;
    instance->array_size = NULL;
    instance->conditional = NULL;
    instance->source_log_name = source_log_name;
    instance->source_line = source_line;
    return instance;
}

static inline struct parser_function_argument_t *parser_function_argument_new (struct rpl_parser_t *parser,
                                                                               kan_interned_string_t source_log_name,
                                                                               kan_rpl_size_t source_line)
{
    struct parser_function_argument_t *function_argument =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_function_argument_t);
    function_argument->next = NULL;
    function_argument->declaration.name = NULL;
    function_argument->declaration.type = NULL;
    function_argument->declaration.array_size_runtime = KAN_FALSE;
    function_argument->declaration.array_size_list = NULL;
    function_argument->access = KAN_RPL_ACCESS_CLASS_READ_ONLY;
    function_argument->conditional = NULL;
    function_argument->source_log_name = source_log_name;
    function_argument->source_line = source_line;
    return function_argument;
}

static inline struct parser_function_t *parser_function_new (struct rpl_parser_t *parser,
                                                             kan_interned_string_t name,
                                                             kan_interned_string_t return_type_name,
                                                             kan_interned_string_t source_log_name,
                                                             kan_rpl_size_t source_line)
{
    struct parser_function_t *instance =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_function_t);
    instance->next = NULL;
    instance->name = name;
    instance->return_type_name = return_type_name;
    instance->first_argument = NULL;
    instance->body_expression = NULL;
    instance->conditional = NULL;
    instance->source_log_name = source_log_name;
    instance->source_line = source_line;
    return instance;
}

static inline void parser_processing_data_init (struct parser_processing_data_t *instance)
{
    instance->first_option = NULL;
    instance->last_option = NULL;
    instance->option_count = 0u;

    instance->first_setting = NULL;
    instance->last_setting = NULL;
    instance->setting_count = 0u;

    instance->first_struct = NULL;
    instance->last_struct = NULL;
    instance->struct_count = 0u;

    instance->first_container = NULL;
    instance->last_container = NULL;
    instance->container_count = 0u;

    instance->first_buffer = NULL;
    instance->last_buffer = NULL;
    instance->buffer_count = 0u;

    instance->first_sampler = NULL;
    instance->last_sampler = NULL;
    instance->sampler_count = 0u;

    instance->first_image = NULL;
    instance->last_image = NULL;
    instance->image_count = 0u;

    instance->first_function = NULL;
    instance->last_function = NULL;
    instance->function_count = 0u;
}

static int re2c_refill_buffer (struct dynamic_parser_state_t *parser)
{
    // We do not refill buffer as we accept only fully loaded shader code files.
    return 1;
}

static inline void re2c_yyskip (struct dynamic_parser_state_t *parser)
{
    if (*parser->cursor == '\n')
    {
        ++parser->cursor_line;
        parser->cursor_symbol = 0u;
    }

    ++parser->cursor;
    ++parser->cursor_symbol;
}

static inline void re2c_yybackup (struct dynamic_parser_state_t *parser)
{
    parser->marker = parser->cursor;
    parser->marker_line = parser->cursor_line;
    parser->marker_symbol = parser->cursor_symbol;
}

static inline void re2c_yyrestore (struct dynamic_parser_state_t *parser)
{
    parser->cursor = parser->marker;
    parser->cursor_line = parser->marker_line;
    parser->cursor_symbol = parser->marker_symbol;
}

static inline void re2c_save_cursor (struct dynamic_parser_state_t *parser)
{
    parser->saved = parser->cursor;
    parser->saved_line = parser->cursor_line;
    parser->saved_symbol = parser->cursor_symbol;
}

static inline void re2c_restore_saved_cursor (struct dynamic_parser_state_t *parser)
{
    parser->cursor = parser->saved;
    parser->cursor_line = parser->saved_line;
    parser->cursor_symbol = parser->saved_symbol;
}

/*!re2c
 re2c:api = custom;
 re2c:api:style = free-form;
 re2c:define:YYCTYPE  = char;
 re2c:define:YYLESSTHAN = "state->limit - state->cursor < @@{len}";
 re2c:define:YYPEEK = "*state->cursor";
 re2c:define:YYSKIP = "re2c_yyskip (state);";
 re2c:define:YYBACKUP = "re2c_yybackup (state);";
 re2c:define:YYRESTORE = "re2c_yyrestore (state);";
 re2c:define:YYFILL   = "re2c_refill_buffer (state) == 0";
 re2c:define:YYSTAGP = "@@{tag} = state->cursor;";
 re2c:define:YYSTAGN = "@@{tag} = NULL;";
 re2c:define:YYSHIFTSTAG  = "@@{tag} += @@{shift};";
 re2c:eof = 0;
 re2c:tags = 1;
 re2c:tags:expression = "state->@@";

 separator = [\x20\x0c\x0a\x0d\x09\x0b];
 identifier = [A-Za-z_][A-Za-z0-9_]*;
 comment = "//" .* "\n";

 setting_block_suffix = "block" separator+ @block_begin [0-9]+ @block_end;
 setting_declaration = "setting" separator+ @name_begin (identifier | ".")+ @name_end;
 setting_flag_on = setting_declaration separator+ "on" (separator+ setting_block_suffix)? separator* ";";
 setting_flag_off = setting_declaration separator+ "off" (separator+ setting_block_suffix)? separator* ";";

 setting_integer_literal = @literal_begin "-"? [0-9]+ @literal_end;
 setting_integer =
     setting_declaration separator+ setting_integer_literal (separator+ setting_block_suffix)? separator* ";";

 setting_floating_literal = @literal_begin "-"? [0-9]+ "." [0-9]+ @literal_end;
 setting_floating =
     setting_declaration separator+ setting_floating_literal (separator+ setting_block_suffix)? separator* ";";

 setting_string_literal = "\"" @literal_begin ((. \ [\x22]) | "\\\"")* @literal_end "\"";
 setting_string =
     setting_declaration separator+ setting_string_literal (separator+ setting_block_suffix)? separator* ";";
 */

static inline struct parser_option_t *rpl_parser_find_option (struct rpl_parser_t *parser, kan_interned_string_t name)
{
    struct parser_option_t *option = parser->processing_data.first_option;
    while (option)
    {
        if (option->name == name)
        {
            return option;
        }

        option = option->next;
    }

    return NULL;
}

static inline kan_bool_t ensure_option_name_unique (struct rpl_parser_t *parser,
                                                    struct dynamic_parser_state_t *state,
                                                    kan_interned_string_t name)
{
    if (rpl_parser_find_option (parser, name))
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Option \"%s\" is already defined.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol, name)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline kan_bool_t parse_main_option_body (struct rpl_parser_t *parser,
                                                 struct dynamic_parser_state_t *state,
                                                 const char *name_begin,
                                                 const char *name_end,
                                                 enum kan_rpl_option_scope_t scope);

static inline kan_rpl_unsigned_int_literal_t parse_unsigned_integer_value (struct rpl_parser_t *parser,
                                                                           struct dynamic_parser_state_t *state,
                                                                           const char *literal_begin,
                                                                           const char *literal_end)
{
    kan_rpl_unsigned_int_literal_t value = 0u;
    for (const char *cursor = literal_begin; cursor < literal_end; ++cursor)
    {
        const kan_rpl_unsigned_int_literal_t old_value = value;
        value = value * 10u + (*cursor - '0');

        if (value < old_value)
        {
            KAN_LOG (rpl_parser, KAN_LOG_WARNING, "[%s:%s] [%ld:%ld]: Found unsigned int literal which is too big.",
                     parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
            return KAN_INT_MAX (kan_rpl_unsigned_int_literal_t);
        }
    }

    return value;
}

static inline float parse_unsigned_floating_value (struct rpl_parser_t *parser,
                                                   struct dynamic_parser_state_t *state,
                                                   const char *literal_begin,
                                                   const char *literal_end)
{
    float value = 0.0f;
    while (literal_begin < literal_end)
    {
        if (*literal_begin == '.')
        {
            ++literal_begin;
            break;
        }

        value = value * 10.0f + (float) (*literal_begin - '0');
        ++literal_begin;
    }

    float modifier = 0.1f;
    while (literal_begin < literal_end)
    {
        value += modifier * (float) (*literal_begin - '0');
        modifier *= 0.1f;
        ++literal_begin;
    }

    return value;
}

static struct parser_expression_tree_node_t *parse_expression (struct rpl_parser_t *parser,
                                                               struct dynamic_parser_state_t *state);

static kan_bool_t parse_call_arguments (struct rpl_parser_t *parser,
                                        struct dynamic_parser_state_t *state,
                                        struct parser_expression_tree_node_t *output);

static struct parser_expression_tree_node_t *parse_detached_conditional (struct rpl_parser_t *parser,
                                                                         struct dynamic_parser_state_t *state)
{
    if (state->detached_conditional)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Encountered conditional after other conditional.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return NULL;
    }

    struct parser_expression_tree_node_t *parsed_node = parse_expression (parser, state);
    if (!parsed_node)
    {
        return NULL;
    }

    if (*state->cursor != ')')
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Expected closing brace \")\" at the end of child grouped expression.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return NULL;
    }

    ++state->cursor;
    return parsed_node;
}

static inline kan_bool_t parse_main_setting_flag (struct rpl_parser_t *parser,
                                                  struct dynamic_parser_state_t *state,
                                                  const char *name_begin,
                                                  const char *name_end,
                                                  kan_bool_t value,
                                                  const char *block_begin,
                                                  const char *block_end)
{
    struct parser_setting_t *node =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_setting_t);

    node->next = NULL;
    if (parser->processing_data.last_setting)
    {
        parser->processing_data.last_setting->next = node;
    }
    else
    {
        parser->processing_data.first_setting = node;
    }

    parser->processing_data.last_setting = node;
    ++parser->processing_data.setting_count;

    node->setting.name = kan_char_sequence_intern (name_begin, name_end);
    node->setting.block =
        block_begin ? parse_unsigned_integer_value (parser, state, block_begin, block_end) : KAN_RPL_SETTING_BLOCK_NONE;
    node->setting.type = KAN_RPL_SETTING_TYPE_FLAG;
    node->setting.flag = value;

    node->setting.conditional = state->detached_conditional;
    state->detached_conditional = NULL;

    node->setting.source_log_name = state->source_log_name;
    node->setting.source_line = state->cursor_line;
    return KAN_TRUE;
}

static inline kan_bool_t parse_main_setting_integer (struct rpl_parser_t *parser,
                                                     struct dynamic_parser_state_t *state,
                                                     const char *name_begin,
                                                     const char *name_end,
                                                     const char *literal_begin,
                                                     const char *literal_end,
                                                     const char *block_begin,
                                                     const char *block_end)
{
    struct parser_setting_t *node =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_setting_t);

    node->next = NULL;
    if (parser->processing_data.last_setting)
    {
        parser->processing_data.last_setting->next = node;
    }
    else
    {
        parser->processing_data.first_setting = node;
    }

    parser->processing_data.last_setting = node;
    ++parser->processing_data.setting_count;

    node->setting.name = kan_char_sequence_intern (name_begin, name_end);
    node->setting.block =
        block_begin ? parse_unsigned_integer_value (parser, state, block_begin, block_end) : KAN_RPL_SETTING_BLOCK_NONE;
    node->setting.type = KAN_RPL_SETTING_TYPE_INTEGER;

    const kan_bool_t negative = *literal_begin == '-';
    if (negative)
    {
        ++literal_begin;
    }

    const kan_rpl_unsigned_int_literal_t unsigned_value =
        parse_unsigned_integer_value (parser, state, literal_begin, literal_end);
    if (unsigned_value > KAN_INT_MAX (kan_rpl_signed_int_literal_t))
    {
        KAN_LOG (rpl_parser, KAN_LOG_WARNING, "[%s:%s] [%ld:%ld]: Setting \"%s\" integer value is too big.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol,
                 node->setting.name)
        return KAN_FALSE;
    }

    node->setting.integer = (kan_rpl_signed_int_literal_t) unsigned_value;
    if (negative)
    {
        node->setting.integer = -node->setting.integer;
    }

    node->setting.conditional = state->detached_conditional;
    state->detached_conditional = NULL;

    node->setting.source_log_name = state->source_log_name;
    node->setting.source_line = state->cursor_line;
    return KAN_TRUE;
}

static inline kan_bool_t parse_main_setting_floating (struct rpl_parser_t *parser,
                                                      struct dynamic_parser_state_t *state,
                                                      const char *name_begin,
                                                      const char *name_end,
                                                      const char *literal_begin,
                                                      const char *literal_end,
                                                      const char *block_begin,
                                                      const char *block_end)
{
    struct parser_setting_t *node =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_setting_t);

    node->next = NULL;
    if (parser->processing_data.last_setting)
    {
        parser->processing_data.last_setting->next = node;
    }
    else
    {
        parser->processing_data.first_setting = node;
    }

    parser->processing_data.last_setting = node;
    ++parser->processing_data.setting_count;

    node->setting.name = kan_char_sequence_intern (name_begin, name_end);
    node->setting.block =
        block_begin ? parse_unsigned_integer_value (parser, state, block_begin, block_end) : KAN_RPL_SETTING_BLOCK_NONE;
    node->setting.type = KAN_RPL_SETTING_TYPE_FLOATING;

    const kan_bool_t negative = *literal_begin == '-';
    if (negative)
    {
        ++literal_begin;
    }

    const float unsigned_value = parse_unsigned_floating_value (parser, state, literal_begin, literal_end);
    node->setting.floating = unsigned_value;

    if (negative)
    {
        node->setting.floating = -node->setting.floating;
    }

    node->setting.conditional = state->detached_conditional;
    state->detached_conditional = NULL;

    node->setting.source_log_name = state->source_log_name;
    node->setting.source_line = state->cursor_line;
    return KAN_TRUE;
}

static inline kan_bool_t parse_main_setting_string (struct rpl_parser_t *parser,
                                                    struct dynamic_parser_state_t *state,
                                                    const char *name_begin,
                                                    const char *name_end,
                                                    const char *literal_begin,
                                                    const char *literal_end,
                                                    const char *block_begin,
                                                    const char *block_end)
{
    struct parser_setting_t *node =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_setting_t);

    node->next = NULL;
    if (parser->processing_data.last_setting)
    {
        parser->processing_data.last_setting->next = node;
    }
    else
    {
        parser->processing_data.first_setting = node;
    }

    parser->processing_data.last_setting = node;
    ++parser->processing_data.setting_count;

    node->setting.name = kan_char_sequence_intern (name_begin, name_end);
    node->setting.block =
        block_begin ? parse_unsigned_integer_value (parser, state, block_begin, block_end) : KAN_RPL_SETTING_BLOCK_NONE;
    node->setting.type = KAN_RPL_SETTING_TYPE_STRING;
    node->setting.string = kan_char_sequence_intern (literal_begin, literal_end);

    node->setting.conditional = state->detached_conditional;
    state->detached_conditional = NULL;

    node->setting.source_log_name = state->source_log_name;
    node->setting.source_line = state->cursor_line;
    return KAN_TRUE;
}

static struct parser_declaration_t *parse_struct_declarations (struct rpl_parser_t *parser,
                                                               struct dynamic_parser_state_t *state);

static inline kan_bool_t parse_main_struct (struct rpl_parser_t *parser,
                                            struct dynamic_parser_state_t *state,
                                            const char *name_begin,
                                            const char *name_end)
{
    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    struct parser_struct_t *new_struct = parser_struct_new (parser, name, state->source_log_name, state->cursor_line);

    new_struct->conditional = state->detached_conditional;
    state->detached_conditional = NULL;
    new_struct->first_declaration = parse_struct_declarations (parser, state);

    if (!new_struct->first_declaration)
    {
        return KAN_FALSE;
    }

    new_struct->next = NULL;
    if (parser->processing_data.last_struct)
    {
        parser->processing_data.last_struct->next = new_struct;
    }
    else
    {
        parser->processing_data.first_struct = new_struct;
    }

    parser->processing_data.last_struct = new_struct;
    ++parser->processing_data.struct_count;
    return KAN_TRUE;
}

static struct parser_container_field_t *parse_container_declarations (struct rpl_parser_t *parser,
                                                                      struct dynamic_parser_state_t *state,
                                                                      kan_bool_t packing_supported);

static inline kan_bool_t parse_main_container (struct rpl_parser_t *parser,
                                               struct dynamic_parser_state_t *state,
                                               enum kan_rpl_container_type_t type,
                                               const char *name_begin,
                                               const char *name_end)
{
    struct parser_container_t *new_container = parser_container_new (
        parser, kan_char_sequence_intern (name_begin, name_end), state->source_log_name, state->cursor_line);
    new_container->type = type;
    new_container->conditional = state->detached_conditional;
    state->detached_conditional = NULL;

    kan_bool_t packing_supported = KAN_FALSE;
    switch (type)
    {
    case KAN_RPL_CONTAINER_TYPE_VERTEX_ATTRIBUTE:
    case KAN_RPL_CONTAINER_TYPE_INSTANCED_ATTRIBUTE:
        packing_supported = KAN_TRUE;
        break;

    case KAN_RPL_CONTAINER_TYPE_STATE:
    case KAN_RPL_CONTAINER_TYPE_COLOR_OUTPUT:
        packing_supported = KAN_FALSE;
        break;
    }

    new_container->first_field = parse_container_declarations (parser, state, packing_supported);
    if (!new_container->first_field)
    {
        return KAN_FALSE;
    }

    new_container->next = NULL;
    if (parser->processing_data.last_container)
    {
        parser->processing_data.last_container->next = new_container;
    }
    else
    {
        parser->processing_data.first_container = new_container;
    }

    parser->processing_data.last_container = new_container;
    ++parser->processing_data.container_count;
    return KAN_TRUE;
}

static inline kan_bool_t parse_main_buffer (struct rpl_parser_t *parser,
                                            struct dynamic_parser_state_t *state,
                                            enum kan_rpl_set_t set,
                                            enum kan_rpl_buffer_type_t type,
                                            const char *name_begin,
                                            const char *name_end)
{
    struct parser_buffer_t *new_buffer = parser_buffer_new (parser, kan_char_sequence_intern (name_begin, name_end),
                                                            set, state->source_log_name, state->cursor_line);
    new_buffer->type = type;
    new_buffer->conditional = state->detached_conditional;
    state->detached_conditional = NULL;
    new_buffer->first_declaration = parse_struct_declarations (parser, state);

    if (!new_buffer->first_declaration)
    {
        return KAN_FALSE;
    }

    new_buffer->next = NULL;
    if (parser->processing_data.last_buffer)
    {
        parser->processing_data.last_buffer->next = new_buffer;
    }
    else
    {
        parser->processing_data.first_buffer = new_buffer;
    }

    parser->processing_data.last_buffer = new_buffer;
    ++parser->processing_data.buffer_count;
    return KAN_TRUE;
}

static kan_bool_t parse_main_sampler (struct rpl_parser_t *parser,
                                      struct dynamic_parser_state_t *state,
                                      enum kan_rpl_set_t set,
                                      const char *name_begin,
                                      const char *name_end);

static kan_bool_t parse_main_image (struct rpl_parser_t *parser,
                                    struct dynamic_parser_state_t *state,
                                    enum kan_rpl_set_t set,
                                    enum kan_rpl_image_type_t type);

static kan_bool_t parse_main_function (struct rpl_parser_t *parser,
                                       struct dynamic_parser_state_t *state,
                                       const char *name_begin,
                                       const char *name_end,
                                       const char *type_name_begin,
                                       const char *type_name_end);

static kan_bool_t parse_main (struct rpl_parser_t *parser, struct dynamic_parser_state_t *state)
{
    state->detached_conditional = NULL;
    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *name_begin;
        const char *name_end;
        const char *literal_begin;
        const char *literal_end;
        const char *type_name_begin;
        const char *type_name_end;
        const char *block_begin;
        const char *block_end;

        const char *marker_set_pass;
        const char *marker_set_material;
        const char *marker_set_object;
        const char *marker_set_shared;

        const char *marker_container_vertex_attribute;
        const char *marker_container_instanced_attribute;
        const char *marker_container_state;
        const char *marker_container_color_output;

        const char *marker_buffer_uniform;
        const char *marker_buffer_read_only_storage;

        const char *marker_image_color_2d;
        const char *marker_image_color_3d;
        const char *marker_image_color_cube;
        const char *marker_image_color_2d_array;
        const char *marker_image_depth_2d;
        const char *marker_image_depth_3d;
        const char *marker_image_depth_cube;
        const char *marker_image_depth_2d_array;

        enum kan_rpl_set_t detected_set;
#define DETECT_SET                                                                                                     \
    if (marker_set_pass)                                                                                               \
    {                                                                                                                  \
        detected_set = KAN_RPL_SET_PASS;                                                                               \
    }                                                                                                                  \
    else if (marker_set_material)                                                                                      \
    {                                                                                                                  \
        detected_set = KAN_RPL_SET_MATERIAL;                                                                           \
    }                                                                                                                  \
    else if (marker_set_object)                                                                                        \
    {                                                                                                                  \
        detected_set = KAN_RPL_SET_OBJECT;                                                                             \
    }                                                                                                                  \
    else if (marker_set_shared)                                                                                        \
    {                                                                                                                  \
        detected_set = KAN_RPL_SET_SHARED;                                                                             \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        detected_set = KAN_RPL_SET_PASS;                                                                               \
        KAN_ASSERT (KAN_FALSE)                                                                                         \
    }

        enum kan_rpl_container_type_t detected_container_type;
#define DETECT_CONTAINER_TYPE                                                                                          \
    if (marker_container_vertex_attribute)                                                                             \
    {                                                                                                                  \
        detected_container_type = KAN_RPL_CONTAINER_TYPE_VERTEX_ATTRIBUTE;                                             \
    }                                                                                                                  \
    else if (marker_container_instanced_attribute)                                                                     \
    {                                                                                                                  \
        detected_container_type = KAN_RPL_CONTAINER_TYPE_INSTANCED_ATTRIBUTE;                                          \
    }                                                                                                                  \
    else if (marker_container_state)                                                                                   \
    {                                                                                                                  \
        detected_container_type = KAN_RPL_CONTAINER_TYPE_STATE;                                                        \
    }                                                                                                                  \
    else if (marker_container_color_output)                                                                            \
    {                                                                                                                  \
        detected_container_type = KAN_RPL_CONTAINER_TYPE_COLOR_OUTPUT;                                                 \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        detected_container_type = KAN_RPL_CONTAINER_TYPE_VERTEX_ATTRIBUTE;                                             \
        KAN_ASSERT (KAN_FALSE)                                                                                         \
    }

        enum kan_rpl_buffer_type_t detected_buffer_type;
#define DETECT_BUFFER_TYPE                                                                                             \
    if (marker_buffer_uniform)                                                                                         \
    {                                                                                                                  \
        detected_buffer_type = KAN_RPL_BUFFER_TYPE_UNIFORM;                                                            \
    }                                                                                                                  \
    else if (marker_buffer_read_only_storage)                                                                          \
    {                                                                                                                  \
        detected_buffer_type = KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE;                                                  \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        detected_buffer_type = KAN_RPL_BUFFER_TYPE_UNIFORM;                                                            \
        KAN_ASSERT (KAN_FALSE)                                                                                         \
    }

        enum kan_rpl_image_type_t detected_image_type;
#define DETECT_IMAGE_TYPE                                                                                              \
    if (marker_image_color_2d)                                                                                         \
    {                                                                                                                  \
        detected_image_type = KAN_RPL_IMAGE_TYPE_COLOR_2D;                                                             \
    }                                                                                                                  \
    else if (marker_image_color_3d)                                                                                    \
    {                                                                                                                  \
        detected_image_type = KAN_RPL_IMAGE_TYPE_COLOR_3D;                                                             \
    }                                                                                                                  \
    else if (marker_image_color_cube)                                                                                  \
    {                                                                                                                  \
        detected_image_type = KAN_RPL_IMAGE_TYPE_COLOR_CUBE;                                                           \
    }                                                                                                                  \
    else if (marker_image_color_2d_array)                                                                              \
    {                                                                                                                  \
        detected_image_type = KAN_RPL_IMAGE_TYPE_COLOR_2D_ARRAY;                                                       \
    }                                                                                                                  \
    else if (marker_image_depth_2d)                                                                                    \
    {                                                                                                                  \
        detected_image_type = KAN_RPL_IMAGE_TYPE_DEPTH_2D;                                                             \
    }                                                                                                                  \
    else if (marker_image_depth_3d)                                                                                    \
    {                                                                                                                  \
        detected_image_type = KAN_RPL_IMAGE_TYPE_DEPTH_3D;                                                             \
    }                                                                                                                  \
    else if (marker_image_depth_cube)                                                                                  \
    {                                                                                                                  \
        detected_image_type = KAN_RPL_IMAGE_TYPE_DEPTH_CUBE;                                                           \
    }                                                                                                                  \
    else if (marker_image_depth_2d_array)                                                                              \
    {                                                                                                                  \
        detected_image_type = KAN_RPL_IMAGE_TYPE_DEPTH_2D_ARRAY;                                                       \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        detected_image_type = KAN_RPL_IMAGE_TYPE_COLOR_2D;                                                             \
        KAN_ASSERT (KAN_FALSE)                                                                                         \
    }

#define CHECKED(...)                                                                                                   \
    if (!(__VA_ARGS__))                                                                                                \
    {                                                                                                                  \
        return KAN_FALSE;                                                                                              \
    }                                                                                                                  \
    continue;

        /*!re2c
         set_prefix =
             ("set_pass" @marker_set_pass) |
             ("set_material" @marker_set_material) |
             ("set_object" @marker_set_object) |
             ("set_shared" @marker_set_shared);

         container_type_prefix =
             ("vertex_attribute_container" @marker_container_vertex_attribute) |
             ("instanced_attribute_container" @marker_container_instanced_attribute) |
             ("state_container" @marker_container_state) |
             ("color_output_container" @marker_container_color_output);

         buffer_type_prefix =
             ("uniform_buffer" @marker_buffer_uniform) |
             ("read_only_storage_buffer" @marker_buffer_read_only_storage);

         image_type_prefix =
             ("image_color_2d" @marker_image_color_2d) |
             ("image_color_3d" @marker_image_color_3d) |
             ("image_color_cube" @marker_image_color_cube) |
             ("image_color_2d_array" @marker_image_color_2d_array) |
             ("image_depth_2d" @marker_image_depth_2d) |
             ("image_depth_3d" @marker_image_depth_3d) |
             ("image_depth_cube" @marker_image_depth_cube) |
             ("image_depth_2d_array" @marker_image_depth_2d_array);

         "global" separator+ @name_begin identifier @name_end separator* ":"
         {
             CHECKED (parse_main_option_body (parser, state, name_begin, name_end, KAN_RPL_OPTION_SCOPE_GLOBAL))
         }

         "instance" separator+ @name_begin identifier @name_end separator* ":"
         {
             CHECKED (parse_main_option_body (parser, state, name_begin, name_end, KAN_RPL_OPTION_SCOPE_INSTANCE))
         }

         "conditional" separator* "("
         {
             state->detached_conditional = parse_detached_conditional (parser, state);
             if (!state->detached_conditional)
             {
                 return KAN_FALSE;
             }

             continue;
         }

         setting_flag_on
         { CHECKED (parse_main_setting_flag (parser, state, name_begin, name_end, KAN_TRUE, block_begin, block_end)) }

         setting_flag_off
         { CHECKED (parse_main_setting_flag (parser, state, name_begin, name_end, KAN_FALSE, block_begin, block_end)) }

         setting_integer
         {
             CHECKED (parse_main_setting_integer (parser, state, name_begin, name_end, literal_begin, literal_end,
                                                  block_begin, block_end))
         }

         setting_floating
         {
             CHECKED (parse_main_setting_floating (parser, state, name_begin, name_end, literal_begin, literal_end,
                                                  block_begin, block_end))
         }

         setting_string
         {
             CHECKED (parse_main_setting_string (parser, state, name_begin, name_end, literal_begin, literal_end,
                                                  block_begin, block_end))
         }

         "struct" separator+ @name_begin identifier @name_end separator* "{"
         { CHECKED (parse_main_struct (parser, state, name_begin, name_end)) }

         container_type_prefix separator+ @name_begin identifier @name_end separator* "{"
         {
             DETECT_CONTAINER_TYPE
             CHECKED (parse_main_container (parser, state, detected_container_type, name_begin, name_end))
         }

         set_prefix separator+ buffer_type_prefix separator+ @name_begin identifier @name_end separator* "{"
         {
             DETECT_SET
             DETECT_BUFFER_TYPE
             CHECKED (parse_main_buffer (parser, state, detected_set, detected_buffer_type, name_begin, name_end))
         }

         set_prefix separator+ "sampler" separator+ @name_begin identifier @name_end separator* ";"
         {
             DETECT_SET
             CHECKED (parse_main_sampler (parser, state, detected_set, name_begin, name_end))
         }

         set_prefix separator+ image_type_prefix
         {
             DETECT_SET
             DETECT_IMAGE_TYPE
             CHECKED (parse_main_image (parser, state, detected_set, detected_image_type))
         }

         @type_name_begin identifier @type_name_end separator+ @name_begin identifier @name_end separator* "("
         { CHECKED (parse_main_function (parser, state, name_begin, name_end, type_name_begin, type_name_end)) }

         separator+ { continue; }
         comment+ { continue; }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression at global scope.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return KAN_FALSE;
         }
         $
         {
             if (state->detached_conditional)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s]: Encountered detached conditional at the end of file.",
                          parser->log_name, state->source_log_name)
                 return KAN_FALSE;
             }

             return KAN_TRUE;
         }
         */

#undef CHECKED
    }
}

static inline kan_bool_t parse_main_option_enum_values (struct rpl_parser_t *parser,
                                                        struct dynamic_parser_state_t *state,
                                                        struct parser_option_t *node)
{
    node->type = KAN_RPL_OPTION_TYPE_ENUM;
    node->first_enum_value = NULL;
    struct parser_option_enum_value_t *last_value_node = NULL;

    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *identifier_begin;
        const char *identifier_end;

        /*!re2c
         ";"
         {
             if (!node->first_enum_value)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Encountered enum with no values.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 return KAN_FALSE;
             }

             return KAN_TRUE;
         }

         @identifier_begin identifier @identifier_end
         {
             kan_interned_string_t value_name = kan_char_sequence_intern (identifier_begin, identifier_end);
             struct parser_option_enum_value_t *node_to_check = node->first_enum_value;

             while (node_to_check)
             {
                 if (node_to_check->name == value_name)
                 {
                     KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                             "[%s:%s] [%ld:%ld]: Value \"%s\" already exists in enum, it cannot be added twice.",
                             parser->log_name, state->source_log_name, (long) state->cursor_line,
                             (long) state->cursor_symbol, value_name)
                     return KAN_FALSE;
                 }

                 node_to_check = node_to_check->next;
             }

             struct parser_option_enum_value_t *value_node =
                 KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_option_enum_value_t);

             if (last_value_node)
             {
                 last_value_node->next = value_node;
             }
             else
             {
                 node->first_enum_value = value_node;
             }

             value_node->next = NULL;
             last_value_node = value_node;
             value_node->name = value_name;
             continue;
         }

         separator+ { continue; }
         comment+ { continue; }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression while reading option enum values.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return KAN_FALSE;
         }

         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while reading option enum values.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return KAN_FALSE;
         }
         */
    }
}

static inline kan_bool_t parse_main_option_body (struct rpl_parser_t *parser,
                                                 struct dynamic_parser_state_t *state,
                                                 const char *name_begin,
                                                 const char *name_end,
                                                 enum kan_rpl_option_scope_t scope)
{
    if (state->detached_conditional)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered conditional before option which is not supported.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    if (!ensure_option_name_unique (parser, state, name))
    {
        return KAN_FALSE;
    }

    struct parser_option_t *node =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_option_t);

    node->next = NULL;
    if (parser->processing_data.last_option)
    {
        parser->processing_data.last_option->next = node;
    }
    else
    {
        parser->processing_data.first_option = node;
    }

    parser->processing_data.last_option = node;
    ++parser->processing_data.option_count;
    node->name = name;
    node->scope = scope;

    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *literal_begin;
        const char *literal_end;

        /*!re2c
         "flag" separator+ "on" separator* ";"
         {
             node->type = KAN_RPL_OPTION_TYPE_FLAG;
             node->flag_default_value = KAN_TRUE;
             return KAN_TRUE;
         }

         "flag" separator+ "off" separator* ";"
         {
             node->type = KAN_RPL_OPTION_TYPE_FLAG;
             node->flag_default_value = KAN_FALSE;
             return KAN_TRUE;
         }

         "uint" separator+ @literal_begin [0-9]+ @literal_end "u"? separator* ";"
         {
             node->type = KAN_RPL_OPTION_TYPE_UINT;
             node->uint_default_value = parse_unsigned_integer_value (parser, state, literal_begin, literal_end);
             return KAN_TRUE;
         }

         "sint" separator+ @literal_begin "-" [0-9]+ @literal_end "s"? separator* ";"
         {
             node->type = KAN_RPL_OPTION_TYPE_SINT;
             kan_bool_t negative = *literal_begin == '-';

             if (negative)
             {
                 ++literal_begin;
             }

             const kan_rpl_unsigned_int_literal_t positive_literal =
                 parse_unsigned_integer_value (parser, state, literal_begin, literal_end);

             if (positive_literal > KAN_INT_MAX (kan_rpl_signed_int_literal_t))
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                     "[%s:%s] [%ld:%ld]: Encountered integer literal that is bigger than maximum allowed %lld.",
                     parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol,
                     (long long) INT64_MAX)
                 return KAN_FALSE;
             }

             node->sint_default_value = (kan_rpl_signed_int_literal_t) positive_literal;
             if (negative)
             {
                 node->sint_default_value = -node->sint_default_value;
             }

             return KAN_TRUE;
         }

         "float" separator+ @literal_begin "-"? [0-9]+ "." [0-9]+  @literal_end separator* ";"
         {
             node->type = KAN_RPL_OPTION_TYPE_FLOAT;
             kan_bool_t negative = *literal_begin == '-';

             if (negative)
             {
                 ++literal_begin;
             }

             node->float_default_value = parse_unsigned_floating_value (parser, state, literal_begin, literal_end);
             if (negative)
             {
                 node->float_default_value = -node->float_default_value;
             }

             return KAN_TRUE;
         }

         "enum"
         {
             return parse_main_option_enum_values (parser, state, node);
         }

         separator+ { continue; }
         comment+ { continue; }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression while reading option body.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return KAN_FALSE;
         }

         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while reading option body.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return KAN_FALSE;
         }
         */
    }
}

static kan_bool_t parse_expression_floating_literal (struct rpl_parser_t *parser,
                                                     struct dynamic_parser_state_t *state,
                                                     struct expression_parse_state_t *expression_parse_state,
                                                     const char *literal_begin,
                                                     const char *literal_end)
{
    if (!expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered floating literal while expecting operation.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    node->type = KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL;
    node->source_log_name = state->source_log_name;
    node->source_line = state->cursor_line;

    const kan_bool_t is_negative = *literal_begin == '-';
    if (is_negative)
    {
        ++literal_begin;
    }

    const float positive_literal = parse_unsigned_floating_value (parser, state, literal_begin, literal_end);
    node->floating_literal = positive_literal;

    if (is_negative)
    {
        node->floating_literal = -node->floating_literal;
    }

    expression_parse_state->expecting_operand = KAN_FALSE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_unsigned_literal (struct rpl_parser_t *parser,
                                                     struct dynamic_parser_state_t *state,
                                                     struct expression_parse_state_t *expression_parse_state,
                                                     const char *literal_begin,
                                                     const char *literal_end)
{
    if (!expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered unsigned literal while expecting operation.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    node->type = KAN_RPL_EXPRESSION_NODE_TYPE_UNSIGNED_LITERAL;
    node->source_log_name = state->source_log_name;
    node->source_line = state->cursor_line;

    if (literal_begin != literal_end && *(literal_end - 1u) == 'u')
    {
        // Remove optional suffix.
        --literal_end;
    }

    node->unsigned_literal = parse_unsigned_integer_value (parser, state, literal_begin, literal_end);
    expression_parse_state->expecting_operand = KAN_FALSE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_signed_literal (struct rpl_parser_t *parser,
                                                   struct dynamic_parser_state_t *state,
                                                   struct expression_parse_state_t *expression_parse_state,
                                                   const char *literal_begin,
                                                   const char *literal_end)
{
    if (!expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Encountered signed literal while expecting operation.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    node->type = KAN_RPL_EXPRESSION_NODE_TYPE_SIGNED_LITERAL;
    node->source_log_name = state->source_log_name;
    node->source_line = state->cursor_line;

    const kan_bool_t is_negative = *literal_begin == '-';
    if (is_negative)
    {
        ++literal_begin;
    }

    if (literal_begin != literal_end && *(literal_end - 1u) == 's')
    {
        // Remove optional suffix.
        --literal_end;
    }

    const kan_rpl_unsigned_int_literal_t positive_literal =
        parse_unsigned_integer_value (parser, state, literal_begin, literal_end);

    if (positive_literal > KAN_INT_MAX (kan_rpl_signed_int_literal_t))
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered integer literal that is bigger than maximum allowed %lld.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol,
                 (long long) INT64_MAX)
        return KAN_FALSE;
    }

    node->signed_literal = (kan_rpl_signed_int_literal_t) positive_literal;
    if (is_negative)
    {
        node->signed_literal = -node->signed_literal;
    }

    expression_parse_state->expecting_operand = KAN_FALSE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_operand_identifier (struct rpl_parser_t *parser,
                                                       struct dynamic_parser_state_t *state,
                                                       struct expression_parse_state_t *expression_parse_state,
                                                       const char *name_begin,
                                                       const char *name_end);

static kan_bool_t parse_expression_binary_operation (struct rpl_parser_t *parser,
                                                     struct dynamic_parser_state_t *state,
                                                     struct expression_parse_state_t *expression_parse_state,
                                                     enum kan_rpl_binary_operation_t operation);

static kan_bool_t parse_expression_field_access (struct rpl_parser_t *parser,
                                                 struct dynamic_parser_state_t *state,
                                                 struct expression_parse_state_t *expression_parse_state,
                                                 const char *name_begin,
                                                 const char *name_end)
{
    if (expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered field access operation while expecting operand.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    return parse_expression_binary_operation (parser, state, expression_parse_state,
                                              KAN_RPL_BINARY_OPERATION_FIELD_ACCESS) &&
           parse_expression_operand_identifier (parser, state, expression_parse_state, name_begin, name_end);
}

static inline void parse_expression_replace_in_parent (struct expression_parse_state_t *expression_parse_state,
                                                       struct parser_expression_tree_node_t *current_child,
                                                       struct parser_expression_tree_node_t *new_child)
{
    if (current_child->parent_expression)
    {
        if (current_child->parent_expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION)
        {
            if (current_child->parent_expression->binary_operation.left_operand_expression == current_child)
            {
                current_child->parent_expression->binary_operation.left_operand_expression = new_child;
            }
            else
            {
                current_child->parent_expression->binary_operation.right_operand_expression = new_child;
            }
        }
        else if (current_child->parent_expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION)
        {
            current_child->parent_expression->unary_operation.operand_expression = new_child;
        }
        else
        {
            // Other parents are not expected for this operation during expression parse.
            KAN_ASSERT (KAN_FALSE)
        }
    }

    if (expression_parse_state->current_node == current_child)
    {
        expression_parse_state->current_node = new_child;
    }
}

static kan_bool_t parse_expression_array_access (struct rpl_parser_t *parser,
                                                 struct dynamic_parser_state_t *state,
                                                 struct expression_parse_state_t *expression_parse_state)
{
    if (expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered field access operation while expecting operand.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    if (!parse_expression_binary_operation (parser, state, expression_parse_state,
                                            KAN_RPL_BINARY_OPERATION_ARRAY_ACCESS))
    {
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    struct parser_expression_tree_node_t *parsed_node = parse_expression (parser, state);

    if (!parsed_node)
    {
        return KAN_FALSE;
    }

    parsed_node->parent_expression = node->parent_expression;
    parse_expression_replace_in_parent (expression_parse_state, node, parsed_node);

    if (*state->cursor != ']')
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Expected closing brace \"]\" at the end of array index expression.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    ++state->cursor;
    expression_parse_state->expecting_operand = KAN_FALSE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_operand_constructor (struct rpl_parser_t *parser,
                                                        struct dynamic_parser_state_t *state,
                                                        struct expression_parse_state_t *expression_parse_state,
                                                        const char *name_begin,
                                                        const char *name_end)
{
    if (!expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered constructor operand while expecting operation.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    node->type = KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR;
    node->source_log_name = state->source_log_name;
    node->source_line = state->cursor_line;
    node->constructor.constructor_type_name = kan_char_sequence_intern (name_begin, name_end);
    node->constructor.arguments = NULL;

    if (!parse_call_arguments (parser, state, node))
    {
        return KAN_FALSE;
    }

    if (*state->cursor != '}')
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Expected closing brace \"}\" at the end of constructor.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    ++state->cursor;
    expression_parse_state->expecting_operand = KAN_FALSE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_operand_function_call (struct rpl_parser_t *parser,
                                                          struct dynamic_parser_state_t *state,
                                                          struct expression_parse_state_t *expression_parse_state,
                                                          const char *name_begin,
                                                          const char *name_end)
{
    if (!expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered function call operand while expecting operation.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    node->type = KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL;
    node->source_log_name = state->source_log_name;
    node->source_line = state->cursor_line;
    node->function_call.function_name = kan_char_sequence_intern (name_begin, name_end);
    node->function_call.arguments = NULL;

    if (!parse_call_arguments (parser, state, node))
    {
        return KAN_FALSE;
    }

    if (*state->cursor != ')')
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Expected closing brace \")\" at the end of function call.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    ++state->cursor;
    expression_parse_state->expecting_operand = KAN_FALSE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_operand_identifier (struct rpl_parser_t *parser,
                                                       struct dynamic_parser_state_t *state,
                                                       struct expression_parse_state_t *expression_parse_state,
                                                       const char *name_begin,
                                                       const char *name_end)
{
    if (!expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Encountered identifier while expecting operation.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    node->type = KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER;
    node->source_log_name = state->source_log_name;
    node->source_line = state->cursor_line;
    node->identifier = kan_char_sequence_intern (name_begin, name_end);
    expression_parse_state->expecting_operand = KAN_FALSE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_operand_child_grouped_expression (
    struct rpl_parser_t *parser,
    struct dynamic_parser_state_t *state,
    struct expression_parse_state_t *expression_parse_state)
{
    if (!expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered child grouped expression while expecting operation.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    struct parser_expression_tree_node_t *parsed_node = parse_expression (parser, state);

    if (!parsed_node)
    {
        return KAN_FALSE;
    }

    parsed_node->parent_expression = node->parent_expression;
    parse_expression_replace_in_parent (expression_parse_state, node, parsed_node);

    if (*state->cursor != ')')
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Expected closing brace \")\" at the end of child grouped expression.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    ++state->cursor;
    expression_parse_state->expecting_operand = KAN_FALSE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_unary_operation (struct rpl_parser_t *parser,
                                                    struct dynamic_parser_state_t *state,
                                                    struct expression_parse_state_t *expression_parse_state,
                                                    enum kan_rpl_unary_operation_t operation)
{
    if (!expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered unary operation while expecting binary operation.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)

    struct parser_expression_tree_node_t *operation_node = parser_expression_tree_node_new (
        parser, KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION, state->source_log_name, state->cursor_line);
    operation_node->unary_operation.unary_operation = operation;

    parse_expression_replace_in_parent (expression_parse_state, node, operation_node);
    operation_node->parent_expression = node->parent_expression;
    node->parent_expression = operation_node;
    operation_node->unary_operation.operand_expression = node;
    expression_parse_state->current_node = node;

    expression_parse_state->expecting_operand = KAN_TRUE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_binary_operation (struct rpl_parser_t *parser,
                                                     struct dynamic_parser_state_t *state,
                                                     struct expression_parse_state_t *expression_parse_state,
                                                     enum kan_rpl_binary_operation_t operation)
{
    if (expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Encountered binary operation while expecting operand.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    const kan_instance_size_t priority = binary_operation_priority[operation];
    struct parser_expression_tree_node_t *operation_node = parser_expression_tree_node_new (
        parser, KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION, state->source_log_name, state->cursor_line);
    operation_node->binary_operation.binary_operation = operation;

    struct parser_expression_tree_node_t *next_operand_placeholder = parser_expression_tree_node_new (
        parser, KAN_RPL_EXPRESSION_NODE_TYPE_NOPE, state->source_log_name, state->cursor_line);
    next_operand_placeholder->parent_expression = operation_node;

    switch (binary_operation_direction[operation])
    {
    case BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT:
    {
        struct parser_expression_tree_node_t *operation_parent =
            expression_parse_state->current_node->parent_expression;
        struct parser_expression_tree_node_t *child_to_replace = expression_parse_state->current_node;

        while (operation_parent)
        {
            if (operation_parent->type == KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION)
            {
                if (priority > binary_operation_priority[operation_parent->binary_operation.binary_operation])
                {
                    break;
                }
            }
            else if (operation_parent->type == KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION)
            {
                if (priority > unary_operation_priority)
                {
                    break;
                }
            }
            else
            {
                // Unexpected type of expression parent node.
                KAN_ASSERT (KAN_FALSE)
            }

            child_to_replace = operation_parent;
            operation_parent = operation_parent->parent_expression;
        }

        parse_expression_replace_in_parent (expression_parse_state, child_to_replace, operation_node);
        operation_node->parent_expression = operation_parent;
        child_to_replace->parent_expression = operation_node;
        operation_node->binary_operation.left_operand_expression = child_to_replace;
        operation_node->binary_operation.right_operand_expression = next_operand_placeholder;
        expression_parse_state->current_node = next_operand_placeholder;

        expression_parse_state->expecting_operand = KAN_TRUE;
        return KAN_TRUE;
    }

    case BINARY_OPERATION_DIRECTION_RIGHT_TO_LEFT:
    {
        struct parser_expression_tree_node_t *operation_parent =
            expression_parse_state->current_node->parent_expression;
        struct parser_expression_tree_node_t *child_to_replace = expression_parse_state->current_node;

        while (operation_parent)
        {
            if (operation_parent->type == KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION)
            {
                if (priority >= binary_operation_priority[operation_parent->binary_operation.binary_operation])
                {
                    break;
                }
            }
            else if (operation_parent->type == KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION)
            {
                if (priority >= unary_operation_priority)
                {
                    break;
                }
            }
            else
            {
                // Unexpected type of expression parent node.
                KAN_ASSERT (KAN_FALSE)
            }

            child_to_replace = operation_parent;
            operation_parent = operation_parent->parent_expression;
        }

        parse_expression_replace_in_parent (expression_parse_state, child_to_replace, operation_node);
        operation_node->parent_expression = operation_parent;
        child_to_replace->parent_expression = operation_node;
        operation_node->binary_operation.left_operand_expression = child_to_replace;
        operation_node->binary_operation.right_operand_expression = next_operand_placeholder;
        expression_parse_state->current_node = next_operand_placeholder;

        expression_parse_state->expecting_operand = KAN_TRUE;
        return KAN_TRUE;
    }
    }

    // Unknown direction.
    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static struct parser_expression_tree_node_t *parse_expression (struct rpl_parser_t *parser,
                                                               struct dynamic_parser_state_t *state)
{
    struct expression_parse_state_t expression_parse_state = {
        .expecting_operand = KAN_TRUE,
        .current_node = parser_expression_tree_node_new (parser, KAN_RPL_EXPRESSION_NODE_TYPE_NOPE,
                                                         state->source_log_name, state->cursor_line),
    };

    while (KAN_TRUE)
    {
        state->token = state->cursor;
        re2c_save_cursor (state);
        const char *name_begin;
        const char *name_end;
        const char *literal_begin;
        const char *literal_end;

#define CHECKED(...)                                                                                                   \
    if (!(__VA_ARGS__))                                                                                                \
    {                                                                                                                  \
        return NULL;                                                                                                   \
    }                                                                                                                  \
    continue;

        /*!re2c
         @literal_begin "-"? [0-9]+ "." [0-9]+  @literal_end
         {
             CHECKED (parse_expression_floating_literal (parser, state, &expression_parse_state,
                                                         literal_begin, literal_end))
         }

         @literal_begin [0-9]+"u" @literal_end
         {
             CHECKED (parse_expression_unsigned_literal (parser, state, &expression_parse_state,
                                                        literal_begin, literal_end))
         }

         @literal_begin "-"? [0-9]+"s"? @literal_end
         {
             CHECKED (parse_expression_signed_literal (parser, state, &expression_parse_state,
                                                        literal_begin, literal_end))
         }

         "." separator* @name_begin identifier @name_end
         {
             CHECKED (parse_expression_field_access (parser, state, &expression_parse_state,
                                                     name_begin, name_end))
         }

         "["
         {
             CHECKED (parse_expression_array_access (parser, state, &expression_parse_state))
         }

         @name_begin identifier @name_end separator* "{" separator*
         {
             CHECKED (parse_expression_operand_constructor (parser, state, &expression_parse_state,
                                                            name_begin, name_end))
         }

         @name_begin identifier @name_end separator* "(" separator*
         {
             CHECKED (parse_expression_operand_function_call (parser, state, &expression_parse_state,
                                                              name_begin, name_end))
         }

         @name_begin identifier @name_end
         {
             CHECKED (parse_expression_operand_identifier (parser, state, &expression_parse_state,
                                                           name_begin, name_end))
         }

         "("
         {
             CHECKED (parse_expression_operand_child_grouped_expression (parser, state, &expression_parse_state))
         }

         "+"
         {
             if (expression_parse_state.expecting_operand)
             {
                 continue;
             }

             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_ADD))
         }

         "-"
         {
             if (expression_parse_state.expecting_operand)
             {
                 CHECKED (parse_expression_unary_operation (parser, state, &expression_parse_state,
                                                            KAN_RPL_UNARY_OPERATION_NEGATE))
             }
             else
             {
                 CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                             KAN_RPL_BINARY_OPERATION_SUBTRACT))
             }
         }

         "*"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_MULTIPLY))
         }

         "/"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_DIVIDE))
         }

         "%"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_MODULUS))
         }

         "="
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_ASSIGN))
         }

         "&&"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_AND))
         }

         "||"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_OR))
         }

         "=="
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_EQUAL))
         }

         "!="
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_NOT_EQUAL))
         }

         "<"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_LESS))
         }

         ">"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_GREATER))
         }

         "<="
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_LESS_OR_EQUAL))
         }

         ">="
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_GREATER_OR_EQUAL))
         }

         "&"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_BITWISE_AND))
         }

         "|"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_BITWISE_OR))
         }

         "^"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_BITWISE_XOR))
         }

         "<<"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_BITWISE_LSHIFT))
         }

         ">>"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_BITWISE_RSHIFT))
         }

         "!"
         {
             CHECKED (parse_expression_unary_operation (parser, state, &expression_parse_state,
                                                        KAN_RPL_UNARY_OPERATION_NOT))
         }

         "~"
         {
             CHECKED (parse_expression_unary_operation (parser, state, &expression_parse_state,
                                                        KAN_RPL_UNARY_OPERATION_BITWISE_NOT))
         }

         separator+ { continue; }
         comment+ { continue; }

         "," | ")" | ";" | "]" | "}"
         {
             // Reached expression break sign, it means that we've finished parsing expression.
             if (expression_parse_state.expecting_operand)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Reached end of expression while waiting for next operand.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)

                 re2c_restore_saved_cursor (state);
                 return NULL;
             }

             re2c_restore_saved_cursor (state);
             struct parser_expression_tree_node_t *state_parent = expression_parse_state.current_node;
             while (state_parent->parent_expression)
             {
                 state_parent = state_parent->parent_expression;
             }

             return state_parent;
         }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown construct while parsing expression.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return NULL;
         }

         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while parsing expression.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return NULL;
         }
         */

#undef CHECKED
    }
}

static kan_bool_t parse_call_arguments (struct rpl_parser_t *parser,
                                        struct dynamic_parser_state_t *state,
                                        struct parser_expression_tree_node_t *output)
{
    KAN_ASSERT (output->type == KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR ||
                output->type == KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL)

    // Special case for calls without arguments.
    if (*state->cursor == ')' || *state->cursor == '}')
    {
        if (output->type == KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR)
        {
            output->constructor.arguments = NULL;
        }
        else if (output->type == KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL)
        {
            output->function_call.arguments = NULL;
        }

        return KAN_TRUE;
    }

    struct parser_expression_list_item_t *previous_item = NULL;

    while (KAN_TRUE)
    {
        struct parser_expression_tree_node_t *expression = parse_expression (parser, state);
        if (!expression)
        {
            return KAN_FALSE;
        }

        struct parser_expression_list_item_t *item =
            KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_expression_list_item_t);

        item->next = NULL;
        item->expression = expression;

        if (previous_item)
        {
            previous_item->next = item;
        }
        else if (output->type == KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR)
        {
            output->constructor.arguments = item;
        }
        else if (output->type == KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL)
        {
            output->function_call.arguments = item;
        }

        previous_item = item;
        if (*state->cursor != ',')
        {
            break;
        }

        ++state->cursor;
    }

    return KAN_TRUE;
}

static kan_bool_t parse_declarations_finish_item (struct rpl_parser_t *parser,
                                                  struct dynamic_parser_state_t *state,
                                                  struct parser_declaration_data_t *declaration,
                                                  kan_bool_t array_size_runtime_allowed)
{
    struct parser_expression_list_item_t *array_size_list_last = NULL;
    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *name_begin;
        const char *name_end;

        /*!re2c
         "["
         {
             if (declaration->array_size_runtime)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Encountered array size expression after runtime size is used.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 return KAN_FALSE;
             }

             struct parser_expression_tree_node_t *array_size_expression = parse_expression (parser, state);
             if (!array_size_expression)
             {
                 return KAN_FALSE;
             }

             struct parser_expression_list_item_t *new_item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                     &parser->allocator, struct parser_expression_list_item_t);

             new_item->next = NULL;
             new_item->expression = array_size_expression;

             if (array_size_list_last)
             {
                 array_size_list_last->next = new_item;
             }
             else
             {
                 declaration->array_size_list = new_item;
             }

             array_size_list_last = new_item;
             if (*state->cursor != ']')
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Encountered array size expression that is not finished by \"]\".",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 return KAN_FALSE;
             }

             ++state->cursor;
             continue;
         }

         "..."
         {
             if (declaration->array_size_runtime)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Encountered array size runtime selector twice.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 return KAN_FALSE;
             }

             if (array_size_list_last)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Encountered array size runtime selector after array size expression.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 return KAN_FALSE;
             }

             if (!array_size_runtime_allowed)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Encountered array size runtime selector in unsupported context.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 return KAN_FALSE;
             }

             declaration->array_size_runtime = KAN_TRUE;
             continue;
         }

         @name_begin identifier @name_end separator*
         {
             declaration->name = kan_char_sequence_intern (name_begin, name_end);
             return KAN_TRUE;
         }

         separator+ { continue; }
         comment+ { continue; }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression while reading declaration list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return KAN_FALSE;
         }

         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while reading declaration list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return KAN_FALSE;
         }
         */
    }
}

static struct parser_declaration_meta_item_t *parse_declarations_meta (struct rpl_parser_t *parser,
                                                                       struct dynamic_parser_state_t *state)
{
    struct parser_declaration_meta_item_t *meta_item = NULL;
    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *name_begin;
        const char *name_end;

        /*!re2c
         @name_begin identifier @name_end separator* ("," | ")")
         {
             struct parser_declaration_meta_item_t *new_item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                     &parser->allocator, struct parser_declaration_meta_item_t);

             new_item->meta = kan_char_sequence_intern (name_begin, name_end);
             new_item->next = meta_item;
             meta_item = new_item;

             const char last = *(state->cursor - 1u);
             if (last == ')')
             {
                 return meta_item;
             }

             continue;
         }

         separator+
         {
             continue;
         }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression while reading meta list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return NULL;
         }
         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while reading meta list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return NULL;
         }
         */

#undef DESTROY_META
    }
}

/*!rules:re2c:field_conditional_and_meta
 "conditional" separator* "("
 {
     state->detached_conditional = parse_detached_conditional (parser, state);
     if (!state->detached_conditional)
     {
         return NULL;
     }

     continue;
 }

 "meta" separator* "("
 {
     struct parser_declaration_meta_item_t *new_meta = parse_declarations_meta (parser, state);
     if (!new_meta)
     {
         return NULL;
     }

     if (detached_meta)
     {
         struct parser_declaration_meta_item_t *last_meta = detached_meta;
         while (last_meta->next)
         {
             last_meta = last_meta->next;
         }

         last_meta->next = new_meta;
     }
     else
     {
         detached_meta = new_meta;
     }

     continue;
 }
*/

static struct parser_declaration_t *parse_struct_declarations (struct rpl_parser_t *parser,
                                                               struct dynamic_parser_state_t *state)
{
    struct parser_declaration_t *first_declaration = NULL;
    struct parser_declaration_t *last_declaration = NULL;
    struct parser_declaration_meta_item_t *detached_meta = NULL;

    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *name_begin;
        const char *name_end;

        /*!re2c
         !use:field_conditional_and_meta;

         @name_begin identifier @name_end
         {
             struct parser_declaration_t *new_declaration = parser_declaration_new (
                     parser, state->source_log_name, state->cursor_line);
             new_declaration->declaration.type = kan_char_sequence_intern (name_begin, name_end);

             if (last_declaration)
             {
                 last_declaration->next = new_declaration;
             }
             else
             {
                 first_declaration = new_declaration;
             }

             last_declaration = new_declaration;
             new_declaration->first_meta = detached_meta;
             detached_meta = NULL;

             new_declaration->conditional = state->detached_conditional;
             state->detached_conditional = NULL;

             if (!parse_declarations_finish_item (parser, state, &new_declaration->declaration, KAN_TRUE))
             {
                 return NULL;
             }

             if (*state->cursor != ';')
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                     "[%s:%s] [%ld:%ld]: Encountered declaration that is not finished by \";\".",
                     parser->log_name, state->source_log_name, (long) state->cursor_line,
                    (long) state->cursor_symbol)
                 return NULL;
             }

              ++state->cursor;
             continue;
         }

         "}" separator* ";"
         {
             if (detached_meta)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                         "[%s:%s] [%ld:%ld]: Encountered detached meta at the end of declaration list.",
                         parser->log_name, state->source_log_name, (long) state->cursor_line,
                         (long) state->cursor_symbol)
                 return NULL;
             }

             return first_declaration;
         }

         separator+ { continue; }
         comment+ { continue; }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression while reading declaration list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return NULL;
         }
         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while reading field declaration list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return NULL;
         }
         */
    }
}

static struct parser_container_field_t *parse_container_declarations (struct rpl_parser_t *parser,
                                                                      struct dynamic_parser_state_t *state,
                                                                      kan_bool_t packing_supported)
{
    struct parser_container_field_t *first_field = NULL;
    struct parser_container_field_t *last_field = NULL;
    struct parser_declaration_meta_item_t *detached_meta = NULL;

    enum kan_rpl_input_pack_class_t pack_class = KAN_RPL_INPUT_PACK_CLASS_DEFAULT;
    kan_rpl_size_t pack_bits = 32u;

    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *type_begin;
        const char *type_end;

        const char *name_begin;
        const char *name_end;

        const char *marker_pack_class_float;
        const char *marker_pack_class_unorm;
        const char *marker_pack_class_snorm;
        const char *marker_pack_class_uint;
        const char *marker_pack_class_sint;

        const char *marker_pack_bits_8;
        const char *marker_pack_bits_16;
        const char *marker_pack_bits_32;

#define DETECT_PACK_CLASS                                                                                              \
    if (marker_pack_class_float)                                                                                       \
    {                                                                                                                  \
        pack_class = KAN_RPL_INPUT_PACK_CLASS_FLOAT;                                                                   \
    }                                                                                                                  \
    else if (marker_pack_class_unorm)                                                                                  \
    {                                                                                                                  \
        pack_class = KAN_RPL_INPUT_PACK_CLASS_UNORM;                                                                   \
    }                                                                                                                  \
    else if (marker_pack_class_snorm)                                                                                  \
    {                                                                                                                  \
        pack_class = KAN_RPL_INPUT_PACK_CLASS_SNORM;                                                                   \
    }                                                                                                                  \
    else if (marker_pack_class_uint)                                                                                   \
    {                                                                                                                  \
        pack_class = KAN_RPL_INPUT_PACK_CLASS_UINT;                                                                    \
    }                                                                                                                  \
    else if (marker_pack_class_sint)                                                                                   \
    {                                                                                                                  \
        pack_class = KAN_RPL_INPUT_PACK_CLASS_SINT;                                                                    \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        KAN_ASSERT (KAN_FALSE)                                                                                         \
        pack_class = KAN_RPL_INPUT_PACK_CLASS_DEFAULT;                                                                 \
    }

#define DETECT_PACK_BITS                                                                                               \
    if (marker_pack_bits_8)                                                                                            \
    {                                                                                                                  \
        pack_bits = 8u;                                                                                                \
    }                                                                                                                  \
    else if (marker_pack_bits_16)                                                                                      \
    {                                                                                                                  \
        pack_bits = 16u;                                                                                               \
    }                                                                                                                  \
    else if (marker_pack_bits_32)                                                                                      \
    {                                                                                                                  \
        pack_bits = 32u;                                                                                               \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        KAN_ASSERT (KAN_FALSE)                                                                                         \
        pack_bits = 32u;                                                                                               \
    }

        /*!re2c
         !use:field_conditional_and_meta;

         pack_class =
             ("float" @marker_pack_class_float) |
             ("unorm" @marker_pack_class_unorm) |
             ("snorm" @marker_pack_class_snorm) |
             ("uint" @marker_pack_class_uint) |
             ("sint" @marker_pack_class_sint);

         pack_bits =
             ("8" @marker_pack_bits_8) |
             ("16" @marker_pack_bits_16) |
             ("32" @marker_pack_bits_32);

         "pack" separator* "(" pack_class pack_bits ")"
         {
             if (!packing_supported)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Encountered pack expression in container that doesn't support it.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 return KAN_FALSE;
             }

             if (pack_class != KAN_RPL_INPUT_PACK_CLASS_DEFAULT)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Encountered attempt to set packing for container field twice.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 return NULL;
             }

             DETECT_PACK_CLASS
             DETECT_PACK_BITS
             continue;
         }

         "pack" separator* "("
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered malformed pack, unable to parse pack type.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return NULL;
         }

         @type_begin identifier @type_end separator+ @name_begin identifier @name_end separator* ";"
         {
             struct parser_container_field_t *new_field = parser_container_field_new (
                     parser, state->source_log_name, state->cursor_line);
             // Containers cannot use arrays, therefore we can parse their name right away after type.
             new_field->declaration.type = kan_char_sequence_intern (type_begin, type_end);
             new_field->declaration.name = kan_char_sequence_intern (name_begin, name_end);

             if (last_field)
             {
                 last_field->next = new_field;
             }
             else
             {
                 first_field = new_field;
             }

             last_field = new_field;

             new_field->pack_class = pack_class;
             new_field->pack_class_bits = pack_bits;

             pack_class = KAN_RPL_INPUT_PACK_CLASS_DEFAULT;
             pack_bits = 0u;

             new_field->first_meta = detached_meta;
             detached_meta = NULL;

             new_field->conditional = state->detached_conditional;
             state->detached_conditional = NULL;
             continue;
         }

         "}" separator* ";"
         {
             if (detached_meta)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                         "[%s:%s] [%ld:%ld]: Encountered detached meta at the end of container field list.",
                         parser->log_name, state->source_log_name, (long) state->cursor_line,
                         (long) state->cursor_symbol)
                 return NULL;
             }

             return first_field;
         }

         separator+ { continue; }
         comment+ { continue; }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression while reading container field list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return NULL;
         }
         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while reading field container field list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return NULL;
         }
         */
    }
}

static kan_bool_t parse_main_sampler (struct rpl_parser_t *parser,
                                      struct dynamic_parser_state_t *state,
                                      enum kan_rpl_set_t set,
                                      const char *name_begin,
                                      const char *name_end)
{
    struct parser_sampler_t *sampler = parser_sampler_new (parser, kan_char_sequence_intern (name_begin, name_end), set,
                                                           state->source_log_name, state->cursor_line);
    sampler->conditional = state->detached_conditional;
    state->detached_conditional = NULL;
    sampler->next = NULL;

    if (parser->processing_data.last_sampler)
    {
        parser->processing_data.last_sampler->next = sampler;
    }
    else
    {
        parser->processing_data.first_sampler = sampler;
    }

    parser->processing_data.last_sampler = sampler;
    ++parser->processing_data.sampler_count;
    return KAN_TRUE;
}

static kan_bool_t parse_main_image_finish_declaration (struct rpl_parser_t *parser,
                                                       struct dynamic_parser_state_t *state,
                                                       struct parser_image_t *image)
{
    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *name_begin;
        const char *name_end;

        /*!re2c
         "["
         {
             if (image->array_size)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Encountered array size expression when image already has array size.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 return KAN_FALSE;
             }

             struct parser_expression_tree_node_t *array_size_expression = parse_expression (parser, state);
             if (!array_size_expression)
             {
                 return KAN_FALSE;
             }

             image->array_size = array_size_expression;
             if (*state->cursor != ']')
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Encountered array size expression that is not finished by \"]\".",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 return KAN_FALSE;
             }

             ++state->cursor;
             continue;
         }

         @name_begin identifier @name_end separator* ";"
         {
             image->name = kan_char_sequence_intern (name_begin, name_end);
             return KAN_TRUE;
         }

         separator+ { continue; }
         comment+ { continue; }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression while parsing image declaration.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return KAN_FALSE;
         }

         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while reading image declaration.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return KAN_FALSE;
         }
         */
    }
}

static kan_bool_t parse_main_image (struct rpl_parser_t *parser,
                                    struct dynamic_parser_state_t *state,
                                    enum kan_rpl_set_t set,
                                    enum kan_rpl_image_type_t type)
{
    struct parser_image_t *image = parser_image_new (parser, set, type, state->source_log_name, state->cursor_line);
    image->conditional = state->detached_conditional;
    state->detached_conditional = NULL;
    image->next = NULL;

    if (!parse_main_image_finish_declaration (parser, state, image))
    {
        return KAN_FALSE;
    }

    if (parser->processing_data.last_image)
    {
        parser->processing_data.last_image->next = image;
    }
    else
    {
        parser->processing_data.first_image = image;
    }

    parser->processing_data.last_image = image;
    ++parser->processing_data.image_count;
    return KAN_TRUE;
}

static struct parser_function_argument_t *parse_function_arguments (struct rpl_parser_t *parser,
                                                                    struct dynamic_parser_state_t *state)
{
    struct parser_function_argument_t *first_argument = NULL;
    struct parser_function_argument_t *last_argument = NULL;
    struct parser_function_argument_meta_item_t *detached_meta = NULL;

    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *name_begin;
        const char *name_end;

        const char *marker_access_in;
        const char *marker_access_out;
        const char *marker_access_in_out;
        enum kan_rpl_access_class_t access;

#define DETECT_ARGUMENT_ACCESS                                                                                         \
    if (marker_access_in)                                                                                              \
    {                                                                                                                  \
        access = KAN_RPL_ACCESS_CLASS_READ_ONLY;                                                                       \
    }                                                                                                                  \
    else if (marker_access_out)                                                                                        \
    {                                                                                                                  \
        access = KAN_RPL_ACCESS_CLASS_WRITE_ONLY;                                                                      \
    }                                                                                                                  \
    else if (marker_access_in_out)                                                                                     \
    {                                                                                                                  \
        access = KAN_RPL_ACCESS_CLASS_READ_WRITE;                                                                      \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        access = KAN_RPL_ACCESS_CLASS_READ_WRITE;                                                                      \
        KAN_ASSERT (KAN_FALSE)                                                                                         \
    }

        /*!re2c
         argument_access =
             ("in" @marker_access_in) |
             ("out" @marker_access_out) |
             ("in" separator+ "out" @marker_access_in_out);

         "conditional" separator* "("
         {
             state->detached_conditional = parse_detached_conditional (parser, state);
             if (!state->detached_conditional)
             {
                 return NULL;
             }

             continue;
         }

         argument_access separator+ @name_begin identifier @name_end
         {
             DETECT_ARGUMENT_ACCESS
             struct parser_function_argument_t *new_argument = parser_function_argument_new (
                     parser, state->source_log_name, state->cursor_line);

             new_argument->access = access;
             new_argument->declaration.type = kan_char_sequence_intern (name_begin, name_end);

             if (last_argument)
             {
                 last_argument->next = new_argument;
             }
             else
             {
                 first_argument = new_argument;
             }

             last_argument = new_argument;
             new_argument->conditional = state->detached_conditional;
             state->detached_conditional = NULL;

             if (!parse_declarations_finish_item (parser, state, &new_argument->declaration, KAN_FALSE))
             {
                 return NULL;
             }

             if (*state->cursor == ')')
             {
                 ++state->cursor;
                 if (detached_meta)
                 {
                     KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                         "[%s:%s] [%ld:%ld]: Encountered detached meta at the end of declaration list.",
                         parser->log_name, state->source_log_name, (long) state->cursor_line,
                         (long) state->cursor_symbol)
                     return NULL;
                 }

                 return first_argument;
             }
             else if (*state->cursor != ',')
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                     "[%s:%s] [%ld:%ld]: Encountered declaration that is not finished by \",\".",
                     parser->log_name, state->source_log_name, (long) state->cursor_line,
                     (long) state->cursor_symbol)
                 return NULL;
             }

             ++state->cursor;
             continue;
         }

         "void" separator* ")"
         {
             if (first_argument)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                         "[%s:%s] [%ld:%ld]: \"void\" argument is only allowed (and required) for functions without"
                          " arguments.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 return NULL;
             }

             // Create special empty declaration to indicate absence of arguments with successful parse.
             first_argument = parser_function_argument_new (parser, state->source_log_name, state->cursor_line);
             first_argument->declaration.type = interned_void;
             return first_argument;
         }

         separator+ { continue; }
         comment+ { continue; }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression while reading declaration list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return NULL;
         }
         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while reading declaration list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return NULL;
         }
         */
    }
}

static struct parser_expression_tree_node_t *parse_scope (struct rpl_parser_t *parser,
                                                          struct dynamic_parser_state_t *state);

static struct parser_expression_tree_node_t *expect_scope (struct rpl_parser_t *parser,
                                                           struct dynamic_parser_state_t *state)
{
    while (KAN_TRUE)
    {
        state->token = state->cursor;
        /*!re2c
         "{" { return parse_scope (parser, state); }
         separator+ { continue; }
         comment+ { continue; }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression while expecting new code scope.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return NULL;
         }
         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while expecting new code scope.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return NULL;
         }
         */
    }
}

static kan_interned_string_t expect_variable_declaration_type (struct rpl_parser_t *parser,
                                                               struct dynamic_parser_state_t *state)
{
    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *name_begin;
        const char *name_end;

        /*!re2c
         @name_begin identifier @name_end { return kan_char_sequence_intern (name_begin, name_end); }

         separator+ { continue; }
         comment+ { continue; }

        *
        {
            KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                     "[%s:%s] [%ld:%ld]: Encountered unknown expression while expecting variable declaration type.",
                     parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
            return NULL;
        }
        $
        {
            KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                     "[%s:%s] [%ld:%ld]: Encountered end of file while expecting variable declaration type.",
                     parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
            return NULL;
        }
        */
    }
}

static struct parser_expression_tree_node_t *expect_variable_declaration (struct rpl_parser_t *parser,
                                                                          struct dynamic_parser_state_t *state)
{
    kan_interned_string_t type_name = expect_variable_declaration_type (parser, state);
    if (!type_name)
    {
        return NULL;
    }

    struct parser_expression_tree_node_t *variable_declaration = parser_expression_tree_node_new (
        parser, KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION, state->source_log_name, state->saved_line);
    variable_declaration->variable_declaration.type = type_name;

    if (!parse_declarations_finish_item (parser, state, &variable_declaration->variable_declaration, KAN_FALSE))
    {
        return NULL;
    }

    if (*state->cursor == '=')
    {
        ++state->cursor;
        struct parser_expression_tree_node_t *assignment = parser_expression_tree_node_new (
            parser, KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION, state->source_log_name, state->saved_line);
        assignment->binary_operation.binary_operation = KAN_RPL_BINARY_OPERATION_ASSIGN;
        assignment->binary_operation.left_operand_expression = variable_declaration;
        assignment->binary_operation.right_operand_expression = parse_expression (parser, state);

        if (!assignment->binary_operation.right_operand_expression)
        {
            return NULL;
        }

        if (*state->cursor != ';')
        {
            KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                     "[%s:%s] [%ld:%ld]: Variable declaration with assignment should be finished with \";\".",
                     parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
            return NULL;
        }

        ++state->cursor;
        return assignment;
    }
    else if (*state->cursor == ';')
    {
        ++state->cursor;
        return variable_declaration;
    }
    else
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Expected either \";\" or \"=\" after variable name in declaration.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return NULL;
    }
}

static struct parser_expression_tree_node_t *parse_if_after_keyword (struct rpl_parser_t *parser,
                                                                     struct dynamic_parser_state_t *state)
{
    struct parser_expression_tree_node_t *condition_expression = parse_expression (parser, state);
    if (!condition_expression)
    {
        return NULL;
    }

    if (*state->cursor != ')')
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Expected \")\" after if condition.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return NULL;
    }

    ++state->cursor;
    struct parser_expression_tree_node_t *if_expression = parser_expression_tree_node_new (
        parser, KAN_RPL_EXPRESSION_NODE_TYPE_IF, state->source_log_name, state->cursor_line);
    if_expression->if_.condition_expression = condition_expression;
    if_expression->if_.true_expression = expect_scope (parser, state);

    if (!if_expression->if_.true_expression)
    {
        return NULL;
    }

    // Match continuation: either else or else if or nothing.
    while (KAN_TRUE)
    {
        state->token = state->cursor;
        re2c_save_cursor (state);

        /*!re2c
         "else" separator+ "if" separator* "("
         {
             struct parser_expression_tree_node_t *next_if = parse_if_after_keyword (parser, state);
             if (!next_if)
             {
                 return NULL;
             }

             // Even in case of else-if false not should technically be a scope.
             struct parser_expression_tree_node_t *scope_expression = parser_expression_tree_node_new (
                     parser, KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE, state->source_log_name, state->saved_line);

             struct parser_expression_list_item_t *new_item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                     &parser->allocator, struct parser_expression_list_item_t);

             new_item->next = NULL;
             new_item->expression = next_if;
             scope_expression->scope_expressions_list = new_item;

             if_expression->if_.false_expression = scope_expression;
             return if_expression;
         }

         "else"
         {
             if_expression->if_.false_expression = expect_scope (parser, state);
             if (!if_expression->if_.false_expression)
             {
                 return NULL;
             }

             return if_expression;
         }

         separator+ { continue; }
         comment+ { continue; }
         * { re2c_restore_saved_cursor (state); return if_expression; }
         $ { re2c_restore_saved_cursor (state); return if_expression; }
         */
    }
}

static struct parser_expression_tree_node_t *parse_for_after_keyword (struct rpl_parser_t *parser,
                                                                      struct dynamic_parser_state_t *state)
{
    struct parser_expression_tree_node_t *for_expression = parser_expression_tree_node_new (
        parser, KAN_RPL_EXPRESSION_NODE_TYPE_FOR, state->source_log_name, state->cursor_line);
    for_expression->for_.init_expression = expect_variable_declaration (parser, state);

    if (!for_expression->for_.init_expression)
    {
        return NULL;
    }

    for_expression->for_.condition_expression = parse_expression (parser, state);
    if (!for_expression->for_.condition_expression)
    {
        return NULL;
    }

    if (*state->cursor != ';')
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Expected \";\" after for condition.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return NULL;
    }

    ++state->cursor;
    for_expression->for_.step_expression = parse_expression (parser, state);

    if (!for_expression->for_.step_expression)
    {
        return NULL;
    }

    if (*state->cursor != ')')
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Expected \")\" after for step expression.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return NULL;
    }

    ++state->cursor;
    for_expression->for_.body_expression = expect_scope (parser, state);

    if (!for_expression->for_.body_expression)
    {
        return NULL;
    }

    return for_expression;
}

static struct parser_expression_tree_node_t *parse_scope (struct rpl_parser_t *parser,
                                                          struct dynamic_parser_state_t *state)
{
    struct parser_expression_tree_node_t *scope_expression = parser_expression_tree_node_new (
        parser, KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE, state->source_log_name, state->saved_line);
    struct parser_expression_list_item_t *last_item = NULL;

#define DOES_NOT_SUPPORT_CONDITIONAL                                                                                   \
    if (state->detached_conditional)                                                                                   \
    {                                                                                                                  \
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: This expression does not support conditional prefix.", \
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)     \
        return NULL;                                                                                                   \
    }

#define CHECK_EXPRESSION_SEMICOLON                                                                                     \
    if (*state->cursor != ';')                                                                                         \
    {                                                                                                                  \
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,                                                                            \
                 "[%s:%s] [%ld:%ld]: Encountered expression that is not finished by \";\" as expected.",               \
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)     \
        return NULL;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    ++state->cursor

#define ADD_EXPRESSION(EXPRESSION)                                                                                     \
    struct parser_expression_list_item_t *new_item =                                                                   \
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->allocator, struct parser_expression_list_item_t);           \
    new_item->next = NULL;                                                                                             \
    new_item->expression = EXPRESSION;                                                                                 \
                                                                                                                       \
    if (last_item)                                                                                                     \
    {                                                                                                                  \
        last_item->next = new_item;                                                                                    \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        scope_expression->scope_expressions_list = new_item;                                                           \
    }                                                                                                                  \
                                                                                                                       \
    last_item = new_item

    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *name_begin;
        const char *name_end;
        re2c_save_cursor (state);

        /*!re2c
         "conditional" separator* "("
         {
             state->detached_conditional = parse_detached_conditional (parser, state);
             if (!state->detached_conditional)
             {
                 return NULL;
             }

             continue;
         }

         ("return" separator+ @name_begin identifier @name_end separator* ";") |
         (identifier separator+ identifier separator* (";" | "=")) |
         (identifier separator* "[")
         {
             DOES_NOT_SUPPORT_CONDITIONAL

             // Parser ad-hoc: "return variable;" looks like a declaration for the parser.
             if (name_begin)
             {
                 struct parser_expression_tree_node_t *return_expression =
                         parser_expression_tree_node_new (parser, KAN_RPL_EXPRESSION_NODE_TYPE_RETURN,
                                                          state->source_log_name, state->saved_line);
                 return_expression->return_expression =
                         parser_expression_tree_node_new (parser, KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER,
                                                          state->source_log_name, state->saved_line);
                 return_expression->return_expression->identifier = kan_char_sequence_intern (name_begin, name_end);

                 ADD_EXPRESSION (return_expression);
                 continue;
             }

             // We've encountered something that definitely looks like part of
             // variable declaration (not a function call of anything else).
             // In this case we restore cursor and read it properly.
             re2c_restore_saved_cursor (state);
             struct parser_expression_tree_node_t *declaration = expect_variable_declaration (parser, state);

             if (!declaration)
             {
                 return NULL;
             }

             ADD_EXPRESSION (declaration);
             continue;
         }

         "{"
         {
             if (state->detached_conditional)
             {
                 struct parser_expression_tree_node_t *expression =
                     parser_expression_tree_node_new (parser, KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE,
                                                      state->source_log_name, state->saved_line);
                 expression->conditional_scope.condition_expression = state->detached_conditional;
                 state->detached_conditional = NULL;
                 expression->conditional_scope.body_expression = parse_scope (parser, state);

                 if (!expression->conditional_scope.body_expression)
                 {
                     return NULL;
                 }

                 ADD_EXPRESSION (expression);
             }
             else
             {
                 struct parser_expression_tree_node_t *new_scope = parse_scope (parser, state);
                 if (!new_scope)
                 {
                     return NULL;
                 }

                 ADD_EXPRESSION (new_scope);
             }

             continue;
         }

         "if" separator* "("
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             struct parser_expression_tree_node_t *expression = parse_if_after_keyword (parser, state);

             if (!expression)
             {
                 return NULL;
             }

             ADD_EXPRESSION (expression);
             continue;
         }

         "for" separator* "("
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             struct parser_expression_tree_node_t *expression = parse_for_after_keyword (parser, state);

             if (!expression)
             {
                 return NULL;
             }

             ADD_EXPRESSION (expression);
             continue;
         }

         "while" separator* "("
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             struct parser_expression_tree_node_t *condition_expression = parse_expression (parser, state);

             if (!condition_expression)
             {
                 return NULL;
             }

             if (*state->cursor != ')')
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: While condition is not finished by \")\" as expected.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 return NULL;
             }

             ++state->cursor;
             struct parser_expression_tree_node_t *body_expression = expect_scope (parser, state);

             if (!body_expression)
             {
                  return NULL;
             }

             struct parser_expression_tree_node_t *while_expression =
                 parser_expression_tree_node_new (parser, KAN_RPL_EXPRESSION_NODE_TYPE_WHILE, state->source_log_name,
                                                  state->saved_line);
             while_expression->while_.condition_expression = condition_expression;
             while_expression->while_.body_expression = body_expression;
             ADD_EXPRESSION (while_expression);
             continue;
         }

         "alias" separator* "(" separator* @name_begin identifier @name_end separator* ","
         {
             if (!state->detached_conditional)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Alias expression must have conditional prefix.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 return NULL;
             }

             struct parser_expression_tree_node_t *body_expression = parse_expression (parser, state);
             if (!body_expression)
             {
                  return NULL;
             }

             struct parser_expression_tree_node_t *alias_expression =
                 parser_expression_tree_node_new (parser, KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS,
                                                  state->source_log_name, state->saved_line);
             alias_expression->conditional_alias.condition_expression = state->detached_conditional;
             state->detached_conditional = NULL;
             alias_expression->conditional_alias.identifier = kan_char_sequence_intern (name_begin, name_end);
             alias_expression->conditional_alias.body_expression = body_expression;
             ADD_EXPRESSION (alias_expression);

             if (*state->cursor != ')')
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Alias expression is not finished by \")\" as expected.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 return NULL;
             }

             ++state->cursor;
             continue;
         }

         "break" separator* ";"
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             struct parser_expression_tree_node_t *break_expression =
                 parser_expression_tree_node_new (parser, KAN_RPL_EXPRESSION_NODE_TYPE_BREAK, state->source_log_name,
                                                  state->saved_line);
             ADD_EXPRESSION (break_expression);
             continue;
         }

         "continue" separator* ";"
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             struct parser_expression_tree_node_t *continue_expression =
                 parser_expression_tree_node_new (parser, KAN_RPL_EXPRESSION_NODE_TYPE_CONTINUE, state->source_log_name,
                                                  state->saved_line);
             ADD_EXPRESSION (continue_expression);
             continue;
         }

         "return" separator* ";"
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             struct parser_expression_tree_node_t *return_expression =
                 parser_expression_tree_node_new (parser, KAN_RPL_EXPRESSION_NODE_TYPE_RETURN, state->source_log_name,
                                                  state->saved_line);
             return_expression->return_expression = NULL;

             ADD_EXPRESSION (return_expression);
             CHECK_EXPRESSION_SEMICOLON;
             continue;
         }

         "return" separator+
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             struct parser_expression_tree_node_t *value_expression = parse_expression (parser, state);
             if (!value_expression)
             {
                  return NULL;
             }

             struct parser_expression_tree_node_t *return_expression =
                 parser_expression_tree_node_new (parser, KAN_RPL_EXPRESSION_NODE_TYPE_RETURN, state->source_log_name,
                                                  state->saved_line);
             return_expression->return_expression = value_expression;

             ADD_EXPRESSION (return_expression);
             CHECK_EXPRESSION_SEMICOLON;
             continue;
         }

         *
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             // Not a prefixed expression (like if, for, etc), parse as just an expression.
             re2c_restore_saved_cursor (state);
             struct parser_expression_tree_node_t *next_expression = parse_expression (parser, state);

             if (!next_expression)
             {
                  return NULL;
             }

             ADD_EXPRESSION (next_expression);
             CHECK_EXPRESSION_SEMICOLON;
             continue;
         }

         "}"
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             // Scope has ended.
             return scope_expression;
         }

         separator+ { continue; }
         comment+ { continue; }

         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while reading scope block.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return NULL;
         }
         */
    }

#undef ADD_EXPRESSION
#undef CHECK_EXPRESSION_SEMICOLON
#undef DOES_NOT_SUPPORT_CONDITIONAL
}

static kan_bool_t parse_main_function (struct rpl_parser_t *parser,
                                       struct dynamic_parser_state_t *state,
                                       const char *name_begin,
                                       const char *name_end,
                                       const char *type_name_begin,
                                       const char *type_name_end)
{
    struct parser_function_t *function = parser_function_new (parser, kan_char_sequence_intern (name_begin, name_end),
                                                              kan_char_sequence_intern (type_name_begin, type_name_end),
                                                              state->source_log_name, state->cursor_line);

    function->conditional = state->detached_conditional;
    state->detached_conditional = NULL;
    function->first_argument = parse_function_arguments (parser, state);

    if (!function->first_argument)
    {
        return KAN_FALSE;
    }

    function->body_expression = expect_scope (parser, state);
    if (function->body_expression)
    {
        function->next = NULL;
        if (parser->processing_data.last_function)
        {
            parser->processing_data.last_function->next = function;
        }
        else
        {
            parser->processing_data.first_function = function;
        }

        parser->processing_data.last_function = function;
        ++parser->processing_data.function_count;
        return KAN_TRUE;
    }

    return KAN_FALSE;
}

void kan_rpl_struct_init (struct kan_rpl_struct_t *instance)
{
    instance->name = NULL;
    kan_dynamic_array_init (&instance->fields, 0u, sizeof (struct kan_rpl_declaration_t),
                            _Alignof (struct kan_rpl_declaration_t), rpl_intermediate_allocation_group);
    instance->conditional_index = KAN_RPL_EXPRESSION_INDEX_NONE;
    instance->source_name = NULL;
    instance->source_line = 0u;
}

void kan_rpl_struct_shutdown (struct kan_rpl_struct_t *instance)
{
    kan_dynamic_array_shutdown (&instance->fields);
}

void kan_rpl_container_init (struct kan_rpl_container_t *instance)
{
    instance->name = NULL;
    instance->type = KAN_RPL_CONTAINER_TYPE_VERTEX_ATTRIBUTE;
    kan_dynamic_array_init (&instance->fields, 0u, sizeof (struct kan_rpl_container_field_t),
                            _Alignof (struct kan_rpl_container_field_t), rpl_intermediate_allocation_group);
    instance->conditional_index = KAN_RPL_EXPRESSION_INDEX_NONE;
    instance->source_name = NULL;
    instance->source_line = 0u;
}

void kan_rpl_container_shutdown (struct kan_rpl_container_t *instance)
{
    kan_dynamic_array_shutdown (&instance->fields);
}

void kan_rpl_buffer_init (struct kan_rpl_buffer_t *instance)
{
    instance->name = NULL;
    instance->type = KAN_RPL_BUFFER_TYPE_UNIFORM;
    kan_dynamic_array_init (&instance->fields, 0u, sizeof (struct kan_rpl_declaration_t),
                            _Alignof (struct kan_rpl_declaration_t), rpl_intermediate_allocation_group);
    instance->conditional_index = KAN_RPL_EXPRESSION_INDEX_NONE;
    instance->source_name = NULL;
    instance->source_line = 0u;
}

void kan_rpl_buffer_shutdown (struct kan_rpl_buffer_t *instance)
{
    kan_dynamic_array_shutdown (&instance->fields);
}

void kan_rpl_sampler_init (struct kan_rpl_sampler_t *instance)
{
    instance->name = NULL;
    instance->set = KAN_RPL_SET_PASS;
    instance->conditional_index = KAN_RPL_EXPRESSION_INDEX_NONE;
    instance->source_name = NULL;
    instance->source_line = 0u;
}

void kan_rpl_image_init (struct kan_rpl_image_t *instance)
{
    instance->name = NULL;
    instance->set = KAN_RPL_SET_PASS;
    instance->type = KAN_RPL_IMAGE_TYPE_COLOR_2D;
    instance->array_size_index = KAN_RPL_EXPRESSION_INDEX_NONE;
    instance->conditional_index = KAN_RPL_EXPRESSION_INDEX_NONE;
    instance->source_name = NULL;
    instance->source_line = 0u;
}

void kan_rpl_function_argument_init (struct kan_rpl_function_argument_t *instance)
{
    instance->type_name = NULL;
    instance->name = NULL;
    instance->access = KAN_RPL_ACCESS_CLASS_READ_ONLY;
    instance->array_size_expression_list_size = KAN_RPL_EXPRESSION_INDEX_NONE;
    instance->array_size_expression_list_index = KAN_RPL_EXPRESSION_INDEX_NONE;
    instance->conditional_index = KAN_RPL_EXPRESSION_INDEX_NONE;
    instance->source_name = NULL;
    instance->source_line = 0u;
}

void kan_rpl_function_init (struct kan_rpl_function_t *instance)
{
    instance->return_type_name = NULL;
    instance->name = NULL;
    kan_dynamic_array_init (&instance->arguments, 0u, sizeof (struct kan_rpl_function_argument_t),
                            _Alignof (struct kan_rpl_function_argument_t), rpl_intermediate_allocation_group);
    instance->body_index = KAN_RPL_EXPRESSION_INDEX_NONE;
    instance->conditional_index = KAN_RPL_EXPRESSION_INDEX_NONE;
    instance->source_name = NULL;
    instance->source_line = 0u;
}

void kan_rpl_function_shutdown (struct kan_rpl_function_t *instance)
{
    kan_dynamic_array_shutdown (&instance->arguments);
}

void kan_rpl_intermediate_init (struct kan_rpl_intermediate_t *instance)
{
    ensure_statics_initialized ();
    kan_dynamic_array_init (&instance->options, 0u, sizeof (struct kan_rpl_option_t),
                            _Alignof (struct kan_rpl_option_t), rpl_intermediate_allocation_group);
    kan_dynamic_array_init (&instance->settings, 0u, sizeof (struct kan_rpl_setting_t),
                            _Alignof (struct kan_rpl_setting_t), rpl_intermediate_allocation_group);
    kan_dynamic_array_init (&instance->structs, 0u, sizeof (struct kan_rpl_struct_t),
                            _Alignof (struct kan_rpl_struct_t), rpl_intermediate_allocation_group);
    kan_dynamic_array_init (&instance->containers, 0u, sizeof (struct kan_rpl_container_t),
                            _Alignof (struct kan_rpl_container_t), rpl_intermediate_allocation_group);
    kan_dynamic_array_init (&instance->buffers, 0u, sizeof (struct kan_rpl_buffer_t),
                            _Alignof (struct kan_rpl_buffer_t), rpl_intermediate_allocation_group);
    kan_dynamic_array_init (&instance->samplers, 0u, sizeof (struct kan_rpl_sampler_t),
                            _Alignof (struct kan_rpl_sampler_t), rpl_intermediate_allocation_group);
    kan_dynamic_array_init (&instance->images, 0u, sizeof (struct kan_rpl_image_t), _Alignof (struct kan_rpl_image_t),
                            rpl_intermediate_allocation_group);
    kan_dynamic_array_init (&instance->functions, 0u, sizeof (struct kan_rpl_function_t),
                            _Alignof (struct kan_rpl_function_t), rpl_intermediate_allocation_group);
    kan_dynamic_array_init (&instance->expression_storage, 0u, sizeof (struct kan_rpl_expression_t),
                            _Alignof (struct kan_rpl_expression_t), rpl_intermediate_allocation_group);
    kan_dynamic_array_init (&instance->expression_lists_storage, 0u, sizeof (kan_rpl_size_t), _Alignof (kan_rpl_size_t),
                            rpl_intermediate_allocation_group);
    kan_dynamic_array_init (&instance->string_lists_storage, 0u, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), rpl_intermediate_allocation_group);
}

void kan_rpl_intermediate_shutdown (struct kan_rpl_intermediate_t *instance)
{
    for (kan_loop_size_t index = 0u; index < instance->structs.size; ++index)
    {
        kan_rpl_struct_shutdown (&((struct kan_rpl_struct_t *) instance->structs.data)[index]);
    }

    for (kan_loop_size_t index = 0u; index < instance->containers.size; ++index)
    {
        kan_rpl_container_shutdown (&((struct kan_rpl_container_t *) instance->containers.data)[index]);
    }

    for (kan_loop_size_t index = 0u; index < instance->buffers.size; ++index)
    {
        kan_rpl_buffer_shutdown (&((struct kan_rpl_buffer_t *) instance->buffers.data)[index]);
    }

    for (kan_loop_size_t index = 0u; index < instance->functions.size; ++index)
    {
        kan_rpl_function_shutdown (&((struct kan_rpl_function_t *) instance->functions.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->options);
    kan_dynamic_array_shutdown (&instance->settings);
    kan_dynamic_array_shutdown (&instance->structs);
    kan_dynamic_array_shutdown (&instance->containers);
    kan_dynamic_array_shutdown (&instance->buffers);
    kan_dynamic_array_shutdown (&instance->samplers);
    kan_dynamic_array_shutdown (&instance->images);
    kan_dynamic_array_shutdown (&instance->functions);
    kan_dynamic_array_shutdown (&instance->expression_storage);
    kan_dynamic_array_shutdown (&instance->expression_lists_storage);
    kan_dynamic_array_shutdown (&instance->string_lists_storage);
}

kan_rpl_parser_t kan_rpl_parser_create (kan_interned_string_t log_name)
{
    ensure_statics_initialized ();
    struct rpl_parser_t *parser = kan_allocate_general (rpl_parser_allocation_group, sizeof (struct rpl_parser_t),
                                                        _Alignof (struct rpl_parser_t));
    parser->log_name = log_name;
    kan_stack_group_allocator_init (&parser->allocator, rpl_parser_allocation_group, KAN_RPL_PARSER_STACK_GROUP_SIZE);
    parser_processing_data_init (&parser->processing_data);
    return KAN_HANDLE_SET (kan_rpl_parser_t, parser);
}

kan_bool_t kan_rpl_parser_add_source (kan_rpl_parser_t parser, const char *source, kan_interned_string_t log_name)
{
    struct rpl_parser_t *instance = KAN_HANDLE_GET (parser);
    struct dynamic_parser_state_t dynamic_state = {
        .source_log_name = log_name,
        .detached_conditional = NULL,
        .limit = source + strlen (source),
        .cursor = source,
        .marker = source,
        .token = source,
        .cursor_line = 1u,
        .cursor_symbol = 0u,
        .marker_line = 1u,
        .marker_symbol = 0u,
        .saved = source,
        .saved_line = 1u,
        .saved_symbol = 0u,
    };

    return parse_main (instance, &dynamic_state);
}

static kan_bool_t build_intermediate_options (struct rpl_parser_t *instance, struct kan_rpl_intermediate_t *output)
{
    kan_dynamic_array_set_capacity (&output->options, instance->processing_data.option_count);
    struct parser_option_t *source_option = instance->processing_data.first_option;

    while (source_option)
    {
        struct kan_rpl_option_t *target_option = kan_dynamic_array_add_last (&output->options);
        KAN_ASSERT (target_option)

        target_option->name = source_option->name;
        target_option->scope = source_option->scope;
        target_option->type = source_option->type;

        switch (source_option->type)
        {
        case KAN_RPL_OPTION_TYPE_FLAG:
            target_option->flag_default_value = source_option->flag_default_value;
            break;

        case KAN_RPL_OPTION_TYPE_UINT:
            target_option->uint_default_value = source_option->uint_default_value;
            break;

        case KAN_RPL_OPTION_TYPE_SINT:
            target_option->sint_default_value = source_option->sint_default_value;
            break;

        case KAN_RPL_OPTION_TYPE_FLOAT:
            target_option->float_default_value = source_option->float_default_value;
            break;

        case KAN_RPL_OPTION_TYPE_ENUM:
        {
            kan_loop_size_t values_count = 0u;
            struct parser_option_enum_value_t *value = source_option->first_enum_value;

            while (value)
            {
                ++values_count;
                value = value->next;
            }

            target_option->enum_values.list_size = (kan_rpl_size_t) values_count;
            target_option->enum_values.list_index = output->string_lists_storage.size;

            if (output->string_lists_storage.size + values_count > output->string_lists_storage.capacity)
            {
                kan_dynamic_array_set_capacity (&output->string_lists_storage, output->string_lists_storage.size * 2u);
            }

            value = source_option->first_enum_value;
            while (value)
            {
                kan_interned_string_t *next_value = kan_dynamic_array_add_last (&output->string_lists_storage);
                KAN_ASSERT (next_value)
                *next_value = value->name;
                value = value->next;
            }

            break;
        }
        }

        source_option = source_option->next;
    }

    return KAN_TRUE;
}

static kan_bool_t build_intermediate_expression (struct rpl_parser_t *instance,
                                                 struct kan_rpl_intermediate_t *intermediate,
                                                 struct parser_expression_tree_node_t *expression,
                                                 kan_rpl_size_t *index_output)
{
    kan_bool_t result = KAN_TRUE;
    *index_output = intermediate->expression_storage.size;
    struct kan_rpl_expression_t *output = kan_dynamic_array_add_last (&intermediate->expression_storage);

    if (!output)
    {
        kan_dynamic_array_set_capacity (&intermediate->expression_storage, intermediate->expression_storage.size * 2u);
        output = kan_dynamic_array_add_last (&intermediate->expression_storage);
        KAN_ASSERT (output)
    }

    output->type = expression->type;
    output->source_name = expression->source_log_name;
    output->source_line = (kan_rpl_size_t) expression->source_line;

#define BUILD_SUB_EXPRESSION(OUTPUT, SOURCE)                                                                           \
    if (!build_intermediate_expression (instance, intermediate, SOURCE, &OUTPUT))                                      \
    {                                                                                                                  \
        result = KAN_FALSE;                                                                                            \
    }

#define COLLECT_LIST_SIZE(NAME, LIST)                                                                                  \
    kan_loop_size_t NAME##_count = 0u;                                                                                 \
    struct parser_expression_list_item_t *NAME##_list = LIST;                                                          \
                                                                                                                       \
    while (NAME##_list)                                                                                                \
    {                                                                                                                  \
        ++NAME##_count;                                                                                                \
        NAME##_list = NAME##_list->next;                                                                               \
    }

#define BUILD_SUB_LIST(COUNT_OUTPUT, INDEX_OUTPUT, COUNT, LIST)                                                        \
    COUNT_OUTPUT = (kan_rpl_size_t) COUNT;                                                                             \
    INDEX_OUTPUT = (kan_rpl_size_t) intermediate->expression_lists_storage.size;                                       \
    struct parser_expression_list_item_t *sub_list = LIST;                                                             \
                                                                                                                       \
    if (intermediate->expression_lists_storage.size + (kan_instance_size_t) COUNT >                                    \
        intermediate->expression_lists_storage.capacity)                                                               \
    {                                                                                                                  \
        kan_dynamic_array_set_capacity (&intermediate->expression_lists_storage,                                       \
                                        intermediate->expression_lists_storage.size * 2u);                             \
    }                                                                                                                  \
                                                                                                                       \
    kan_rpl_size_t *sub_list_index_output = &(                                                                         \
        (kan_rpl_size_t *) intermediate->expression_lists_storage.data)[intermediate->expression_lists_storage.size];  \
    intermediate->expression_lists_storage.size += (kan_instance_size_t) COUNT;                                        \
                                                                                                                       \
    while (sub_list)                                                                                                   \
    {                                                                                                                  \
        if (!build_intermediate_expression (instance, intermediate, sub_list->expression, sub_list_index_output))      \
        {                                                                                                              \
            result = KAN_FALSE;                                                                                        \
        }                                                                                                              \
                                                                                                                       \
        ++sub_list_index_output;                                                                                       \
        sub_list = sub_list->next;                                                                                     \
    }

    switch (expression->type)
    {
    case KAN_RPL_EXPRESSION_NODE_TYPE_NOPE:
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER:
        output->identifier = expression->identifier;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL:
        output->floating_literal = expression->floating_literal;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_UNSIGNED_LITERAL:
        output->unsigned_literal = expression->unsigned_literal;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_SIGNED_LITERAL:
        output->signed_literal = expression->signed_literal;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION:
    {
        output->variable_declaration.type_name = expression->variable_declaration.type;
        output->variable_declaration.variable_name = expression->variable_declaration.name;

        COLLECT_LIST_SIZE (dimension, expression->variable_declaration.array_size_list)
        BUILD_SUB_LIST (output->variable_declaration.array_size_expression_list_size,
                        output->variable_declaration.array_size_expression_list_index, dimension_count,
                        expression->variable_declaration.array_size_list)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION:
    {
        output->binary_operation.operation = expression->binary_operation.binary_operation;
        BUILD_SUB_EXPRESSION (output->binary_operation.left_operand_index,
                              expression->binary_operation.left_operand_expression)
        BUILD_SUB_EXPRESSION (output->binary_operation.right_operand_index,
                              expression->binary_operation.right_operand_expression)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION:
    {
        output->unary_operation.operation = expression->unary_operation.unary_operation;
        BUILD_SUB_EXPRESSION (output->unary_operation.operand_index, expression->unary_operation.operand_expression)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE:
    {
        COLLECT_LIST_SIZE (expression, expression->scope_expressions_list)
        BUILD_SUB_LIST (output->scope.statement_list_size, output->scope.statement_list_index, expression_count,
                        expression->scope_expressions_list)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL:
    {
        output->function_call.name = expression->function_call.function_name;
        COLLECT_LIST_SIZE (argument, expression->function_call.arguments)
        BUILD_SUB_LIST (output->function_call.argument_list_size, output->function_call.argument_list_index,
                        argument_count, expression->function_call.arguments)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR:
    {
        output->constructor.type_name = expression->constructor.constructor_type_name;
        COLLECT_LIST_SIZE (argument, expression->constructor.arguments)
        BUILD_SUB_LIST (output->constructor.argument_list_size, output->constructor.argument_list_index, argument_count,
                        expression->constructor.arguments)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_IF:
    {
        BUILD_SUB_EXPRESSION (output->if_.condition_index, expression->if_.condition_expression)
        BUILD_SUB_EXPRESSION (output->if_.true_index, expression->if_.true_expression)

        if (expression->if_.false_expression)
        {
            BUILD_SUB_EXPRESSION (output->if_.false_index, expression->if_.false_expression)
        }
        else
        {
            output->if_.false_index = KAN_RPL_EXPRESSION_INDEX_NONE;
        }

        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FOR:
    {
        BUILD_SUB_EXPRESSION (output->for_.init_index, expression->for_.init_expression)
        BUILD_SUB_EXPRESSION (output->for_.condition_index, expression->for_.condition_expression)
        BUILD_SUB_EXPRESSION (output->for_.step_index, expression->for_.step_expression)
        BUILD_SUB_EXPRESSION (output->for_.body_index, expression->for_.body_expression)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_WHILE:
    {
        BUILD_SUB_EXPRESSION (output->while_.condition_index, expression->while_.condition_expression)
        BUILD_SUB_EXPRESSION (output->while_.body_index, expression->while_.body_expression)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE:
    {
        BUILD_SUB_EXPRESSION (output->conditional_scope.condition_index,
                              expression->conditional_scope.condition_expression)
        BUILD_SUB_EXPRESSION (output->conditional_scope.body_index, expression->conditional_scope.body_expression)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS:
    {
        output->conditional_alias.name = expression->conditional_alias.identifier;
        BUILD_SUB_EXPRESSION (output->conditional_alias.condition_index,
                              expression->conditional_alias.condition_expression)
        BUILD_SUB_EXPRESSION (output->conditional_alias.expression_index, expression->conditional_alias.body_expression)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_BREAK:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONTINUE:
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_RETURN:
    {
        if (expression->return_expression)
        {
            BUILD_SUB_EXPRESSION (output->return_index, expression->return_expression)
        }
        else
        {
            output->return_index = KAN_RPL_EXPRESSION_INDEX_NONE;
        }

        break;
    }
    }

#undef BUILD_SUB_EXPRESSION
#undef COLLECT_LIST_SIZE
#undef BUILD_SUB_LIST

    return result;
}

static kan_bool_t build_intermediate_setting (struct rpl_parser_t *instance,
                                              struct kan_rpl_intermediate_t *intermediate,
                                              struct parser_setting_data_t *setting,
                                              struct kan_rpl_setting_t *output)
{
    output->name = setting->name;
    output->block = setting->block;
    output->type = setting->type;
    output->conditional_index = KAN_RPL_EXPRESSION_INDEX_NONE;
    output->source_name = setting->source_log_name;
    output->source_line = (kan_rpl_size_t) setting->source_line;

    switch (setting->type)
    {
    case KAN_RPL_SETTING_TYPE_FLAG:
        output->flag = setting->flag;
        break;

    case KAN_RPL_SETTING_TYPE_INTEGER:
        output->integer = setting->integer;
        break;

    case KAN_RPL_SETTING_TYPE_FLOATING:
        output->floating = setting->floating;
        break;

    case KAN_RPL_SETTING_TYPE_STRING:
        output->string = setting->string;
        break;
    }

    if (setting->conditional)
    {
        if (!build_intermediate_expression (instance, intermediate, setting->conditional, &output->conditional_index))
        {
            return KAN_FALSE;
        }
    }

    return KAN_TRUE;
}

static kan_bool_t build_intermediate_settings (struct rpl_parser_t *instance, struct kan_rpl_intermediate_t *output)
{
    kan_dynamic_array_set_capacity (&output->settings, instance->processing_data.setting_count);
    struct parser_setting_t *source_setting = instance->processing_data.first_setting;
    kan_bool_t result = KAN_TRUE;

    while (source_setting)
    {
        struct kan_rpl_setting_t *target_setting = kan_dynamic_array_add_last (&output->settings);
        KAN_ASSERT (target_setting)

        if (!build_intermediate_setting (instance, output, &source_setting->setting, target_setting))
        {
            result = KAN_FALSE;
        }

        source_setting = source_setting->next;
    }

    return result;
}

static kan_bool_t build_array_sizes (struct rpl_parser_t *instance,
                                     struct kan_rpl_intermediate_t *intermediate,
                                     struct parser_expression_list_item_t *array_size_list,
                                     kan_rpl_size_t *output_array_size_expression_list_size,
                                     kan_rpl_size_t *output_array_size_expression_list_index)
{
    kan_bool_t result = KAN_TRUE;
    kan_loop_size_t array_sizes_count = 0u;
    struct parser_expression_list_item_t *array_size = array_size_list;

    while (array_size)
    {
        ++array_sizes_count;
        array_size = array_size->next;
    }

    *output_array_size_expression_list_size = (kan_rpl_size_t) array_sizes_count;
    *output_array_size_expression_list_index = intermediate->expression_lists_storage.size;

    if (intermediate->expression_lists_storage.size + array_sizes_count >
        intermediate->expression_lists_storage.capacity)
    {
        kan_dynamic_array_set_capacity (&intermediate->expression_lists_storage,
                                        intermediate->expression_lists_storage.size * 2u);
    }

    array_size = array_size_list;
    while (array_size)
    {
        kan_rpl_size_t *new_index = kan_dynamic_array_add_last (&intermediate->expression_lists_storage);
        KAN_ASSERT (new_index)

        if (!build_intermediate_expression (instance, intermediate, array_size->expression, new_index))
        {
            result = KAN_FALSE;
        }

        array_size = array_size->next;
    }

    return result;
}

static void build_meta (struct rpl_parser_t *instance,
                        struct kan_rpl_intermediate_t *intermediate,
                        struct parser_declaration_meta_item_t *first_meta,
                        kan_rpl_size_t *output_meta_list_size,
                        kan_rpl_size_t *output_meta_list_index)
{
    kan_loop_size_t meta_count = 0u;
    struct parser_declaration_meta_item_t *meta_item = first_meta;

    while (meta_item)
    {
        ++meta_count;
        meta_item = meta_item->next;
    }

    *output_meta_list_size = (kan_rpl_size_t) meta_count;
    *output_meta_list_index = intermediate->string_lists_storage.size;

    if (intermediate->string_lists_storage.size + meta_count > intermediate->string_lists_storage.capacity)
    {
        kan_dynamic_array_set_capacity (&intermediate->string_lists_storage,
                                        intermediate->string_lists_storage.size * 2u);
    }

    meta_item = first_meta;
    while (meta_item)
    {
        kan_interned_string_t *next_meta = kan_dynamic_array_add_last (&intermediate->string_lists_storage);
        KAN_ASSERT (next_meta)
        *next_meta = meta_item->meta;
        meta_item = meta_item->next;
    }
}

static kan_bool_t build_struct_field_declarations (struct rpl_parser_t *instance,
                                                   struct kan_rpl_intermediate_t *intermediate,
                                                   struct parser_declaration_t *first_declaration,
                                                   struct kan_dynamic_array_t *output)
{
    kan_bool_t result = KAN_TRUE;
    kan_loop_size_t count = 0u;
    struct parser_declaration_t *declaration = first_declaration;

    while (declaration)
    {
        ++count;
        declaration = declaration->next;
    }

    kan_dynamic_array_set_capacity (output, count);
    declaration = first_declaration;

    while (declaration)
    {
        struct kan_rpl_declaration_t *new_declaration = kan_dynamic_array_add_last (output);
        KAN_ASSERT (new_declaration)

        new_declaration->name = declaration->declaration.name;
        new_declaration->type_name = declaration->declaration.type;
        new_declaration->conditional_index = KAN_RPL_EXPRESSION_INDEX_NONE;
        new_declaration->source_name = declaration->source_log_name;
        new_declaration->source_line = (kan_rpl_size_t) declaration->source_line;
        new_declaration->array_size_runtime = declaration->declaration.array_size_runtime;

        result &= build_array_sizes (instance, intermediate, declaration->declaration.array_size_list,
                                     &new_declaration->array_size_expression_list_size,
                                     &new_declaration->array_size_expression_list_index);

        build_meta (instance, intermediate, declaration->first_meta, &new_declaration->meta_list_size,
                    &new_declaration->meta_list_index);

        if (declaration->conditional &&
            !build_intermediate_expression (instance, intermediate, declaration->conditional,
                                            &new_declaration->conditional_index))
        {
            result = KAN_FALSE;
        }

        declaration = declaration->next;
    }

    return result;
}

static kan_bool_t build_intermediate_structs (struct rpl_parser_t *instance, struct kan_rpl_intermediate_t *output)
{
    kan_bool_t result = KAN_TRUE;
    kan_dynamic_array_set_capacity (&output->structs, instance->processing_data.struct_count);
    struct parser_struct_t *struct_data = instance->processing_data.first_struct;

    while (struct_data)
    {
        struct kan_rpl_struct_t *new_struct = kan_dynamic_array_add_last (&output->structs);
        KAN_ASSERT (new_struct)

        kan_rpl_struct_init (new_struct);
        new_struct->name = struct_data->name;
        new_struct->source_name = struct_data->source_log_name;
        new_struct->source_line = (kan_rpl_size_t) struct_data->source_line;

        if (!build_struct_field_declarations (instance, output, struct_data->first_declaration, &new_struct->fields))
        {
            result = KAN_FALSE;
        }

        if (struct_data->conditional &&
            !build_intermediate_expression (instance, output, struct_data->conditional, &new_struct->conditional_index))
        {
            result = KAN_FALSE;
        }

        struct_data = struct_data->next;
    }

    return result;
}

static kan_bool_t build_container_field_declarations (struct rpl_parser_t *instance,
                                                      struct kan_rpl_intermediate_t *intermediate,
                                                      struct parser_container_field_t *first_field,
                                                      struct kan_dynamic_array_t *output)
{
    kan_bool_t result = KAN_TRUE;
    kan_loop_size_t count = 0u;
    struct parser_container_field_t *field = first_field;

    while (field)
    {
        ++count;
        field = field->next;
    }

    kan_dynamic_array_set_capacity (output, count);
    field = first_field;

    while (field)
    {
        struct kan_rpl_container_field_t *new_field = kan_dynamic_array_add_last (output);
        KAN_ASSERT (new_field)

        new_field->name = field->declaration.name;
        new_field->type_name = field->declaration.type;
        new_field->pack_class = field->pack_class;
        new_field->pack_class_bits = field->pack_class_bits;
        new_field->conditional_index = KAN_RPL_EXPRESSION_INDEX_NONE;
        new_field->source_name = field->source_log_name;
        new_field->source_line = (kan_rpl_size_t) field->source_line;
        build_meta (instance, intermediate, field->first_meta, &new_field->meta_list_size, &new_field->meta_list_index);

        if (field->conditional &&
            !build_intermediate_expression (instance, intermediate, field->conditional, &new_field->conditional_index))
        {
            result = KAN_FALSE;
        }

        field = field->next;
    }

    return result;
}

static kan_bool_t build_intermediate_containers (struct rpl_parser_t *instance, struct kan_rpl_intermediate_t *output)
{
    kan_bool_t result = KAN_TRUE;
    kan_dynamic_array_set_capacity (&output->containers, instance->processing_data.container_count);
    struct parser_container_t *source_container = instance->processing_data.first_container;

    while (source_container)
    {
        struct kan_rpl_container_t *target_container = kan_dynamic_array_add_last (&output->containers);
        KAN_ASSERT (target_container)

        kan_rpl_container_init (target_container);
        target_container->name = source_container->name;
        target_container->type = source_container->type;
        target_container->source_name = source_container->source_log_name;
        target_container->source_line = (kan_rpl_size_t) source_container->source_line;

        if (!build_container_field_declarations (instance, output, source_container->first_field,
                                                 &target_container->fields))
        {
            result = KAN_FALSE;
        }

        if (source_container->conditional &&
            !build_intermediate_expression (instance, output, source_container->conditional,
                                            &target_container->conditional_index))
        {
            result = KAN_FALSE;
        }

        source_container = source_container->next;
    }

    return result;
}

static kan_bool_t build_intermediate_buffers (struct rpl_parser_t *instance, struct kan_rpl_intermediate_t *output)
{
    kan_bool_t result = KAN_TRUE;
    kan_dynamic_array_set_capacity (&output->buffers, instance->processing_data.buffer_count);
    struct parser_buffer_t *source_buffer = instance->processing_data.first_buffer;

    while (source_buffer)
    {
        struct kan_rpl_buffer_t *target_buffer = kan_dynamic_array_add_last (&output->buffers);
        KAN_ASSERT (target_buffer)

        kan_rpl_buffer_init (target_buffer);
        target_buffer->name = source_buffer->name;
        target_buffer->set = source_buffer->set;
        target_buffer->type = source_buffer->type;
        target_buffer->source_name = source_buffer->source_log_name;
        target_buffer->source_line = (kan_rpl_size_t) source_buffer->source_line;

        if (!build_struct_field_declarations (instance, output, source_buffer->first_declaration,
                                              &target_buffer->fields))
        {
            result = KAN_FALSE;
        }

        if (source_buffer->conditional && !build_intermediate_expression (instance, output, source_buffer->conditional,
                                                                          &target_buffer->conditional_index))
        {
            result = KAN_FALSE;
        }

        source_buffer = source_buffer->next;
    }

    return result;
}

static kan_bool_t build_intermediate_samplers (struct rpl_parser_t *instance, struct kan_rpl_intermediate_t *output)
{
    kan_bool_t result = KAN_TRUE;
    kan_dynamic_array_set_capacity (&output->samplers, instance->processing_data.sampler_count);
    struct parser_sampler_t *source_sampler = instance->processing_data.first_sampler;

    while (source_sampler)
    {
        struct kan_rpl_sampler_t *target_sampler = kan_dynamic_array_add_last (&output->samplers);
        KAN_ASSERT (target_sampler)

        kan_rpl_sampler_init (target_sampler);
        target_sampler->name = source_sampler->name;
        target_sampler->set = source_sampler->set;
        target_sampler->source_name = source_sampler->source_log_name;
        target_sampler->source_line = (kan_rpl_size_t) source_sampler->source_line;

        if (source_sampler->conditional &&
            !build_intermediate_expression (instance, output, source_sampler->conditional,
                                            &target_sampler->conditional_index))
        {
            result = KAN_FALSE;
        }

        source_sampler = source_sampler->next;
    }

    return result;
}

static kan_bool_t build_intermediate_images (struct rpl_parser_t *instance, struct kan_rpl_intermediate_t *output)
{
    kan_bool_t result = KAN_TRUE;
    kan_dynamic_array_set_capacity (&output->images, instance->processing_data.image_count);
    struct parser_image_t *source_image = instance->processing_data.first_image;

    while (source_image)
    {
        struct kan_rpl_image_t *target_image = kan_dynamic_array_add_last (&output->images);
        KAN_ASSERT (target_image)

        kan_rpl_image_init (target_image);
        target_image->name = source_image->name;
        target_image->set = source_image->set;
        target_image->type = source_image->type;
        target_image->source_name = source_image->source_log_name;
        target_image->source_line = (kan_rpl_size_t) source_image->source_line;

        if (source_image->array_size && !build_intermediate_expression (instance, output, source_image->array_size,
                                                                        &target_image->array_size_index))
        {
            result = KAN_FALSE;
        }

        if (source_image->conditional && !build_intermediate_expression (instance, output, source_image->conditional,
                                                                         &target_image->conditional_index))
        {
            result = KAN_FALSE;
        }

        source_image = source_image->next;
    }

    return result;
}

static kan_bool_t build_function_arguments (struct rpl_parser_t *instance,
                                            struct kan_rpl_intermediate_t *intermediate,
                                            struct parser_function_argument_t *first_argument,
                                            struct kan_dynamic_array_t *output)
{
    kan_bool_t result = KAN_TRUE;
    kan_loop_size_t count = 0u;
    struct parser_function_argument_t *argument = first_argument;

    while (argument)
    {
        ++count;
        argument = argument->next;
    }

    kan_dynamic_array_set_capacity (output, count);
    argument = first_argument;

    while (argument)
    {
        struct kan_rpl_function_argument_t *new_argument = kan_dynamic_array_add_last (output);
        KAN_ASSERT (new_argument)

        new_argument->name = argument->declaration.name;
        new_argument->type_name = argument->declaration.type;
        new_argument->access = argument->access;
        new_argument->conditional_index = KAN_RPL_EXPRESSION_INDEX_NONE;
        new_argument->source_name = argument->source_log_name;
        new_argument->source_line = (kan_rpl_size_t) argument->source_line;

        // Should not be allowed by parsing.
        KAN_ASSERT (!argument->declaration.array_size_runtime)

        result &= build_array_sizes (instance, intermediate, argument->declaration.array_size_list,
                                     &new_argument->array_size_expression_list_size,
                                     &new_argument->array_size_expression_list_index);

        if (argument->conditional && !build_intermediate_expression (instance, intermediate, argument->conditional,
                                                                     &new_argument->conditional_index))
        {
            result = KAN_FALSE;
        }

        argument = argument->next;
    }

    return result;
}

static kan_bool_t build_intermediate_functions (struct rpl_parser_t *instance, struct kan_rpl_intermediate_t *output)
{
    kan_bool_t result = KAN_TRUE;
    kan_dynamic_array_set_capacity (&output->functions, instance->processing_data.function_count);
    struct parser_function_t *function = instance->processing_data.first_function;

    while (function)
    {
        struct kan_rpl_function_t *new_function = kan_dynamic_array_add_last (&output->functions);
        KAN_ASSERT (new_function)

        kan_rpl_function_init (new_function);
        new_function->return_type_name = function->return_type_name;
        new_function->name = function->name;
        new_function->source_name = function->source_log_name;
        new_function->source_line = (kan_rpl_size_t) function->source_line;

        // Special case -- void function.
        if (function->first_argument->declaration.type != interned_void)
        {
            if (!build_function_arguments (instance, output, function->first_argument, &new_function->arguments))
            {
                result = KAN_FALSE;
            }
        }

        if (!build_intermediate_expression (instance, output, function->body_expression, &new_function->body_index))
        {
            result = KAN_FALSE;
        }

        if (function->conditional &&
            !build_intermediate_expression (instance, output, function->conditional, &new_function->conditional_index))
        {
            result = KAN_FALSE;
        }

        function = function->next;
    }

    return result;
}

kan_bool_t kan_rpl_parser_build_intermediate (kan_rpl_parser_t parser, struct kan_rpl_intermediate_t *output)
{
    struct rpl_parser_t *instance = KAN_HANDLE_GET (parser);
    output->log_name = instance->log_name;

    kan_dynamic_array_set_capacity (&output->expression_storage, KAN_RPL_INTERMEDIATE_EXPRESSION_STORAGE_SIZE);
    kan_dynamic_array_set_capacity (&output->expression_lists_storage,
                                    KAN_RPL_INTERMEDIATE_EXPRESSION_LISTS_STORAGE_SIZE);
    kan_dynamic_array_set_capacity (&output->string_lists_storage, KAN_RPL_INTERMEDIATE_META_LISTS_STORAGE_SIZE);

    const kan_bool_t result =
        build_intermediate_options (instance, output) && build_intermediate_settings (instance, output) &&
        build_intermediate_structs (instance, output) && build_intermediate_containers (instance, output) &&
        build_intermediate_buffers (instance, output) && build_intermediate_samplers (instance, output) &&
        build_intermediate_images (instance, output) && build_intermediate_functions (instance, output);

    kan_dynamic_array_set_capacity (&output->expression_storage, output->expression_storage.size);
    kan_dynamic_array_set_capacity (&output->expression_lists_storage, output->expression_lists_storage.size);
    kan_dynamic_array_set_capacity (&output->string_lists_storage, output->string_lists_storage.size);
    return result;
}

void kan_rpl_parser_destroy (kan_rpl_parser_t parser)
{
    struct rpl_parser_t *instance = KAN_HANDLE_GET (parser);
    kan_stack_group_allocator_shutdown (&instance->allocator);
    kan_free_general (rpl_parser_allocation_group, instance, sizeof (struct rpl_parser_t));
}
