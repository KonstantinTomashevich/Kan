#define KAN_RPL_COMPILER_IMPLEMENTATION
#include <kan/render_pipeline_language/compiler_internal.h>

enum conditional_evaluation_result_t
{
    CONDITIONAL_EVALUATION_RESULT_FAILED = 0u,
    CONDITIONAL_EVALUATION_RESULT_TRUE,
    CONDITIONAL_EVALUATION_RESULT_FALSE,
};

enum compile_time_evaluation_value_type_t
{
    CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR = 0u,
    CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN,
    CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING,

    /// \brief For the simplification, we treat both unsigned and signed integers
    ///        as the same type during compile time evaluation.
    CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER,
};

struct compile_time_evaluation_value_t
{
    enum compile_time_evaluation_value_type_t type;
    union
    {
        kan_bool_t boolean_value;
        kan_rpl_signed_int_literal_t integer_value;
        float floating_value;
    };
};

struct resolve_expression_alias_node_t
{
    struct resolve_expression_alias_node_t *next;
    kan_interned_string_t name;

    // Compiler instance expressions do not have links to parents, therefore we can safely resolve alias once and
    // paste it as a link to every detected usage.
    struct compiler_instance_expression_node_t *resolved_expression;
};

struct resolve_expression_scope_t
{
    struct resolve_expression_scope_t *parent;
    struct compiler_instance_function_node_t *function;
    struct resolve_expression_alias_node_t *first_alias;
    struct compiler_instance_expression_node_t *associated_resolved_scope_if_any;
    struct compiler_instance_expression_node_t *associated_outer_loop_if_any;
};

struct resolve_fiend_access_linear_node_t
{
    struct resolve_fiend_access_linear_node_t *next;
    struct kan_rpl_expression_t *field_source;
};

static struct compile_time_evaluation_value_t evaluate_compile_time_expression (
    struct rpl_compiler_context_t *instance,
    struct kan_rpl_intermediate_t *intermediate,
    struct kan_rpl_expression_t *expression,
    kan_bool_t instance_options_allowed)
{
    struct compile_time_evaluation_value_t result = {
        .type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR,
        .integer_value = 0,
    };

    switch (expression->type)
    {
    case KAN_RPL_EXPRESSION_NODE_TYPE_NOPE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION:
    case KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_IF:
    case KAN_RPL_EXPRESSION_NODE_TYPE_FOR:
    case KAN_RPL_EXPRESSION_NODE_TYPE_WHILE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS:
    case KAN_RPL_EXPRESSION_NODE_TYPE_BREAK:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONTINUE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_RETURN:
        // Should not be allowed by parser.
        KAN_ASSERT (KAN_FALSE)
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER:
    {
        kan_bool_t found = KAN_FALSE;
        for (kan_loop_size_t option_index = 0u; option_index < instance->option_values.size; ++option_index)
        {
            struct rpl_compiler_context_option_value_t *option =
                &((struct rpl_compiler_context_option_value_t *) instance->option_values.data)[option_index];

            if (option->name == expression->identifier)
            {
                found = KAN_TRUE;
                if (option->scope == KAN_RPL_OPTION_SCOPE_INSTANCE && !instance_options_allowed)
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Compile time expression contains non-global option \"%s\" in context that "
                             "only allows global options.",
                             instance->log_name, intermediate->log_name, expression->source_name,
                             (long) expression->source_line, expression->identifier)
                    result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
                }
                else
                {
                    switch (option->type)
                    {
                    case KAN_RPL_OPTION_TYPE_FLAG:
                        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN;
                        result.boolean_value = option->flag_value;
                        break;

                    case KAN_RPL_OPTION_TYPE_COUNT:
                        if (option->count_value > KAN_INT_MAX (kan_rpl_unsigned_int_literal_t))
                        {
                            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                                     "[%s:%s:%s:%ld] Compile time expression uses count option \"%s\" that has value "
                                     "%llu that is greater that supported in conditionals.",
                                     instance->log_name, intermediate->log_name, expression->source_name,
                                     (long) expression->source_line, expression->identifier,
                                     (unsigned long long) option->count_value)
                            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
                        }
                        else
                        {
                            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER;
                            result.integer_value = (kan_rpl_signed_int_literal_t) option->count_value;
                        }
                        break;
                    }
                }

                break;
            }
        }

        if (!found)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s%ld] Compile time expression uses option \"%s\" that cannot be found.",
                     instance->log_name, intermediate->log_name, expression->source_name,
                     (long) expression->source_line, expression->identifier)
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
        }

        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL:
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING;
        result.floating_value = expression->floating_literal;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_UNSIGNED_LITERAL:
        if (expression->unsigned_literal <= (kan_rpl_unsigned_int_literal_t) INT32_MAX)
        {
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER;
            result.integer_value = (kan_rpl_signed_int_literal_t) expression->unsigned_literal;
        }
        else
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s%ld] Compile time expression uses unsigned literal %llu which is too big.",
                     instance->log_name, intermediate->log_name, expression->source_name,
                     (long) expression->source_line, (unsigned long long) expression->unsigned_literal)
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
        }

        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_SIGNED_LITERAL:
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER;
        result.integer_value = expression->signed_literal;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION:
    {
        struct compile_time_evaluation_value_t left_operand = evaluate_compile_time_expression (
            instance, intermediate,
            &((struct kan_rpl_expression_t *)
                  intermediate->expression_storage.data)[expression->binary_operation.left_operand_index],
            instance_options_allowed);

        struct compile_time_evaluation_value_t right_operand = evaluate_compile_time_expression (
            instance, intermediate,
            &((struct kan_rpl_expression_t *)
                  intermediate->expression_storage.data)[expression->binary_operation.right_operand_index],
            instance_options_allowed);

        if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR ||
            right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR)
        {
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            break;
        }

#define EVALUATE_NUMERIC_OPERATION(OPERATOR)                                                                           \
    if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER &&                                              \
        right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)                                               \
    {                                                                                                                  \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER;                                                       \
        result.integer_value = left_operand.integer_value OPERATOR right_operand.integer_value;                        \
    }                                                                                                                  \
    else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING &&                                        \
             right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING)                                         \
    {                                                                                                                  \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING;                                                      \
        result.floating_value = left_operand.floating_value OPERATOR right_operand.floating_value;                     \
    }                                                                                                                  \
    else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER &&                                         \
             right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING)                                         \
    {                                                                                                                  \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING;                                                      \
        result.floating_value = (float) left_operand.integer_value OPERATOR right_operand.floating_value;              \
    }                                                                                                                  \
    else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING &&                                        \
             right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)                                          \
    {                                                                                                                  \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING;                                                      \
        result.floating_value = left_operand.floating_value OPERATOR (float) right_operand.integer_value;              \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Operator \"%s\" has unsupported operand types.", \
                 instance->log_name, intermediate->log_name, expression->source_name, (long) expression->source_line,  \
                 #OPERATOR)                                                                                            \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;                                                         \
    }

#define EVALUATE_COMPARISON_OPERATION(OPERATOR)                                                                        \
    result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN;                                                           \
    if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER &&                                              \
        right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)                                               \
    {                                                                                                                  \
        result.boolean_value = left_operand.integer_value OPERATOR right_operand.integer_value;                        \
    }                                                                                                                  \
    else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING &&                                        \
             right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING)                                         \
    {                                                                                                                  \
        result.boolean_value = left_operand.floating_value OPERATOR right_operand.floating_value;                      \
    }                                                                                                                  \
    else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER &&                                         \
             right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING)                                         \
    {                                                                                                                  \
        result.boolean_value = (float) left_operand.integer_value OPERATOR right_operand.floating_value;               \
    }                                                                                                                  \
    else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING &&                                        \
             right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)                                          \
    {                                                                                                                  \
        result.boolean_value = left_operand.floating_value OPERATOR (float) right_operand.integer_value;               \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Operator \"%s\" has unsupported operand types.", \
                 instance->log_name, intermediate->log_name, expression->source_name, (long) expression->source_line,  \
                 #OPERATOR)                                                                                            \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;                                                         \
    }

#define EVALUATE_BITWISE_OPERATION(OPERATOR)                                                                           \
    if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER &&                                              \
        right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)                                               \
    {                                                                                                                  \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER;                                                       \
        result.integer_value = left_operand.integer_value OPERATOR right_operand.integer_value;                        \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Operator \"%s\" has unsupported operand types.", \
                 instance->log_name, intermediate->log_name, expression->source_name, (long) expression->source_line,  \
                 #OPERATOR)                                                                                            \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;                                                         \
    }

        switch (expression->binary_operation.operation)
        {
        case KAN_RPL_BINARY_OPERATION_FIELD_ACCESS:
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Operator \".\" is not supported in compile time expressions.", instance->log_name,
                     intermediate->log_name, expression->source_name, (long) expression->source_line)
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            break;

        case KAN_RPL_BINARY_OPERATION_ARRAY_ACCESS:
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Operator \"[]\" is not supported in compile time expressions.", instance->log_name,
                     intermediate->log_name, expression->source_name, (long) expression->source_line)
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            break;

        case KAN_RPL_BINARY_OPERATION_ADD:
            EVALUATE_NUMERIC_OPERATION (+)
            break;

        case KAN_RPL_BINARY_OPERATION_SUBTRACT:
            EVALUATE_NUMERIC_OPERATION (-)
            break;

        case KAN_RPL_BINARY_OPERATION_MULTIPLY:
            EVALUATE_NUMERIC_OPERATION (*)
            break;

        case KAN_RPL_BINARY_OPERATION_DIVIDE:
            EVALUATE_NUMERIC_OPERATION (/)
            break;

        case KAN_RPL_BINARY_OPERATION_MODULUS:
            if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER &&
                right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)
            {
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER;
                result.integer_value = left_operand.integer_value % right_operand.integer_value;
            }
            else
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Operator \"%%\" has unsupported operand types.", instance->log_name,
                         instance->log_name, expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            }

            break;

        case KAN_RPL_BINARY_OPERATION_ASSIGN:
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Operator \"=\" is not supported in conditionals.", instance->log_name,
                     instance->log_name, expression->source_name, (long) expression->source_line)
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            break;

        case KAN_RPL_BINARY_OPERATION_AND:
            if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN &&
                right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN)
            {
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN;
                result.boolean_value = left_operand.boolean_value && right_operand.boolean_value;
            }
            else
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Operator \"&&\" has unsupported operand types.", instance->log_name,
                         instance->log_name, expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            }

            break;

        case KAN_RPL_BINARY_OPERATION_OR:
            if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN &&
                right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN)
            {
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN;
                result.boolean_value = left_operand.boolean_value || right_operand.boolean_value;
            }
            else
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Operator \"||\" has unsupported operand types.", instance->log_name,
                         instance->log_name, expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            }

            break;

        case KAN_RPL_BINARY_OPERATION_EQUAL:
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN;
            if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER &&
                right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)
            {
                result.boolean_value = left_operand.integer_value == right_operand.integer_value;
            }
            else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN &&
                     right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN)
            {
                result.boolean_value = left_operand.boolean_value == right_operand.boolean_value;
            }
            else
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Operator \"==\" has unsupported operand types.", instance->log_name,
                         instance->log_name, expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            }

            break;

        case KAN_RPL_BINARY_OPERATION_NOT_EQUAL:
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN;
            if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER &&
                right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)
            {
                result.boolean_value = left_operand.integer_value != right_operand.integer_value;
            }
            else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN &&
                     right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN)
            {
                result.boolean_value = left_operand.boolean_value != right_operand.boolean_value;
            }
            else
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Operator \"!=\" has unsupported operand types.", instance->log_name,
                         instance->log_name, expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            }

            break;

        case KAN_RPL_BINARY_OPERATION_LESS:
            EVALUATE_COMPARISON_OPERATION (<)
            break;

        case KAN_RPL_BINARY_OPERATION_GREATER:
            EVALUATE_COMPARISON_OPERATION (>)
            break;

        case KAN_RPL_BINARY_OPERATION_LESS_OR_EQUAL:
            EVALUATE_COMPARISON_OPERATION (<=)
            break;

        case KAN_RPL_BINARY_OPERATION_GREATER_OR_EQUAL:
            EVALUATE_COMPARISON_OPERATION (>=)
            break;

        case KAN_RPL_BINARY_OPERATION_BITWISE_AND:
            EVALUATE_BITWISE_OPERATION (&)
            break;

        case KAN_RPL_BINARY_OPERATION_BITWISE_OR:
            EVALUATE_BITWISE_OPERATION (|)
            break;

        case KAN_RPL_BINARY_OPERATION_BITWISE_XOR:
            EVALUATE_BITWISE_OPERATION (^)
            break;

        case KAN_RPL_BINARY_OPERATION_BITWISE_LSHIFT:
            EVALUATE_BITWISE_OPERATION (<<)
            break;

        case KAN_RPL_BINARY_OPERATION_BITWISE_RSHIFT:
            EVALUATE_BITWISE_OPERATION (>>)
            break;
        }

#undef EVALUATE_NUMERIC_OPERATION
#undef EVALUATE_COMPARISON_OPERATION
#undef EVALUATE_BITWISE_OPERATION

        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION:
    {
        struct compile_time_evaluation_value_t operand = evaluate_compile_time_expression (
            instance, intermediate,
            &((struct kan_rpl_expression_t *)
                  intermediate->expression_storage.data)[expression->unary_operation.operand_index],
            instance_options_allowed);
        result.type = operand.type;

        switch (expression->unary_operation.operation)
        {
        case KAN_RPL_UNARY_OPERATION_NEGATE:
            switch (operand.type)
            {
            case CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR:
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN:
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Operator \"-\" cannot be applied to boolean value.", instance->log_name,
                         instance->log_name, expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER:
                result.integer_value = -operand.integer_value;
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING:
                result.floating_value = -operand.floating_value;
                break;
            }

            break;

        case KAN_RPL_UNARY_OPERATION_NOT:
            switch (operand.type)
            {
            case CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR:
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN:
                result.boolean_value = !operand.boolean_value;
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER:
            case CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING:
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Operator \"!\" can only be applied to boolean value.", instance->log_name,
                         instance->log_name, expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
                break;
            }

            break;

        case KAN_RPL_UNARY_OPERATION_BITWISE_NOT:
            switch (operand.type)
            {
            case CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR:
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER:
                result.integer_value = ~operand.integer_value;
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN:
            case CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING:
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Operator \"~\" can only be applied to integer value.", instance->log_name,
                         instance->log_name, expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
                break;
            }

            break;
        }

        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL:
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Compile time expression contains function call which is not supported.",
                 instance->log_name, instance->log_name, expression->source_name, (long) expression->source_line)
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR:
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Compile time expression contains constructor which is not supported.",
                 instance->log_name, instance->log_name, expression->source_name, (long) expression->source_line)
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
        break;
    }

    return result;
}

static enum conditional_evaluation_result_t evaluate_conditional (struct rpl_compiler_context_t *instance,
                                                                  struct kan_rpl_intermediate_t *intermediate,
                                                                  kan_rpl_size_t conditional_index,
                                                                  kan_bool_t instance_options_allowed)
{
    if (conditional_index == KAN_RPL_EXPRESSION_INDEX_NONE)
    {
        return CONDITIONAL_EVALUATION_RESULT_TRUE;
    }

    struct kan_rpl_expression_t *expression =
        &((struct kan_rpl_expression_t *) intermediate->expression_storage.data)[conditional_index];

    if (expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    {
        return CONDITIONAL_EVALUATION_RESULT_TRUE;
    }

    struct compile_time_evaluation_value_t result =
        evaluate_compile_time_expression (instance, intermediate, expression, instance_options_allowed);

    switch (result.type)
    {
    case CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR:
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Conditional evaluation resulted in failure.",
                 instance->log_name, intermediate->log_name, expression->source_name, (long) expression->source_line)
        return CONDITIONAL_EVALUATION_RESULT_FAILED;

    case CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN:
        return result.boolean_value ? CONDITIONAL_EVALUATION_RESULT_TRUE : CONDITIONAL_EVALUATION_RESULT_FALSE;

    case CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER:
        return result.integer_value != 0 ? CONDITIONAL_EVALUATION_RESULT_TRUE : CONDITIONAL_EVALUATION_RESULT_FALSE;

    case CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING:
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Conditional evaluation resulted in floating value.", instance->log_name,
                 intermediate->log_name, expression->source_name, (long) expression->source_line)
        return CONDITIONAL_EVALUATION_RESULT_FAILED;
    }

    KAN_ASSERT (KAN_FALSE)
    return CONDITIONAL_EVALUATION_RESULT_FAILED;
}

static kan_bool_t resolve_settings (struct rpl_compiler_context_t *context,
                                    struct rpl_compiler_instance_t *instance,
                                    struct kan_rpl_intermediate_t *intermediate,
                                    struct kan_dynamic_array_t *settings_array,
                                    kan_bool_t instance_options_allowed,
                                    struct compiler_instance_setting_node_t **first_output,
                                    struct compiler_instance_setting_node_t **last_output)
{
    kan_bool_t result = KAN_TRUE;
    for (kan_loop_size_t setting_index = 0u; setting_index < settings_array->size; ++setting_index)
    {
        struct kan_rpl_setting_t *source_setting = &((struct kan_rpl_setting_t *) settings_array->data)[setting_index];

        switch (
            evaluate_conditional (context, intermediate, source_setting->conditional_index, instance_options_allowed))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            result = KAN_FALSE;
            break;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
        {
            struct compiler_instance_setting_node_t *target_setting = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                &instance->resolve_allocator, struct compiler_instance_setting_node_t);

            target_setting->next = NULL;
            target_setting->name = source_setting->name;
            target_setting->block = source_setting->block;
            target_setting->type = source_setting->type;
            target_setting->module_name = intermediate->log_name;
            target_setting->source_name = source_setting->source_name;
            target_setting->source_line = source_setting->source_line;

            switch (source_setting->type)
            {
            case KAN_RPL_SETTING_TYPE_FLAG:
                target_setting->flag = source_setting->flag;
                break;

            case KAN_RPL_SETTING_TYPE_INTEGER:
                target_setting->integer = source_setting->integer;
                break;

            case KAN_RPL_SETTING_TYPE_FLOATING:
                target_setting->floating = source_setting->floating;
                break;

            case KAN_RPL_SETTING_TYPE_STRING:
                target_setting->string = source_setting->string;
                break;
            }

            if (*last_output)
            {
                (*last_output)->next = target_setting;
                (*last_output) = target_setting;
            }
            else
            {
                (*first_output) = target_setting;
                (*last_output) = target_setting;
            }

            break;
        }

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            break;
        }
    }

    return result;
}

static inline kan_bool_t resolve_array_dimension_value (struct rpl_compiler_context_t *context,
                                                        struct rpl_compiler_instance_t *instance,
                                                        struct kan_rpl_intermediate_t *intermediate,
                                                        kan_rpl_size_t expression_index,
                                                        kan_rpl_size_t *output,
                                                        kan_bool_t instance_options_allowed,
                                                        kan_rpl_size_t log_dimension_index)
{
    struct kan_rpl_expression_t *expression =
        &((struct kan_rpl_expression_t *) intermediate->expression_storage.data)[expression_index];

    struct compile_time_evaluation_value_t value =
        evaluate_compile_time_expression (context, intermediate, expression, instance_options_allowed);

    switch (value.type)
    {
    case CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR:
        return KAN_FALSE;

    case CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN:
    case CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING:
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Declaration array size at dimension %ld calculation resulted in non-integer value.",
                 context->log_name, intermediate->log_name, expression->source_name, (long) expression->source_line,
                 (long) log_dimension_index)
        return KAN_FALSE;

    case CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER:
        if (value.integer_value > 0 && (kan_rpl_size_t) value.integer_value <= KAN_INT_MAX (kan_rpl_size_t))
        {
            *output = (kan_rpl_size_t) value.integer_value;
            return KAN_TRUE;
        }
        else
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Declaration array size at dimension %ld calculation resulted in invalid value for "
                     "array size %ld.",
                     context->log_name, intermediate->log_name, expression->source_name, (long) expression->source_line,
                     (long) log_dimension_index, (long) value.integer_value)
            return KAN_FALSE;
        }
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t resolve_array_dimensions (struct rpl_compiler_context_t *context,
                                                   struct rpl_compiler_instance_t *instance,
                                                   struct kan_rpl_intermediate_t *intermediate,
                                                   struct compiler_instance_variable_t *variable,
                                                   kan_bool_t array_size_runtime,
                                                   kan_rpl_size_t dimensions_list_size,
                                                   kan_rpl_size_t dimensions_list_index,
                                                   kan_bool_t instance_options_allowed)
{
    kan_bool_t result = KAN_TRUE;
    variable->type.array_size_runtime = array_size_runtime;
    variable->type.array_dimensions_count = dimensions_list_size;

    if (variable->type.array_dimensions_count > 0u)
    {
        // Should not be allowed by parser.
        KAN_ASSERT (!array_size_runtime)

        variable->type.array_dimensions = kan_stack_group_allocator_allocate (
            &instance->resolve_allocator, sizeof (kan_instance_size_t) * variable->type.array_dimensions_count,
            _Alignof (kan_instance_size_t));

        for (kan_loop_size_t dimension = 0u; dimension < variable->type.array_dimensions_count; ++dimension)
        {
            const kan_rpl_size_t expression_index =
                ((kan_rpl_size_t *) intermediate->expression_lists_storage.data)[dimensions_list_index + dimension];

            result &= resolve_array_dimension_value (context, instance, intermediate, expression_index,
                                                     &variable->type.array_dimensions[dimension],
                                                     instance_options_allowed, (kan_rpl_size_t) dimension);
        }
    }
    else
    {
        variable->type.array_dimensions = NULL;
    }

    return result;
}

static kan_bool_t resolve_use_struct (struct rpl_compiler_context_t *context,
                                      struct rpl_compiler_instance_t *instance,
                                      kan_interned_string_t name,
                                      struct compiler_instance_struct_node_t **output);

static inline kan_bool_t resolve_type (struct rpl_compiler_context_t *context,
                                       struct rpl_compiler_instance_t *instance,
                                       kan_interned_string_t intermediate_log_name,
                                       struct compiler_instance_type_definition_t *type,
                                       kan_interned_string_t type_name,
                                       const char *declaration_name_for_logging,
                                       kan_interned_string_t source_name_for_logging,
                                       kan_rpl_size_t source_line_for_logging)
{
    // We do not resolve anything except for base type here. Clean everything else just in case.
    type->access = KAN_RPL_ACCESS_CLASS_READ_ONLY;
    type->flags = 0u;

    type->array_size_runtime = KAN_FALSE;
    type->array_dimensions_count = 0u;
    type->array_dimensions = NULL;

    if (type_name == STATICS.interned_void)
    {
        type->class = COMPILER_INSTANCE_TYPE_CLASS_VOID;
        return KAN_TRUE;
    }

    struct inbuilt_vector_type_t *vector_type = find_inbuilt_vector_type (type_name);
    if (vector_type)
    {
        type->class = COMPILER_INSTANCE_TYPE_CLASS_VECTOR;
        type->vector_data = vector_type;
        return KAN_TRUE;
    }

    struct inbuilt_matrix_type_t *matrix_type = find_inbuilt_matrix_type (type_name);
    if (matrix_type)
    {
        type->class = COMPILER_INSTANCE_TYPE_CLASS_MATRIX;
        type->matrix_data = matrix_type;
        return KAN_TRUE;
    }

    if (type_name == STATICS.interned_sampler)
    {
        type->class = COMPILER_INSTANCE_TYPE_CLASS_SAMPLER;
        return KAN_TRUE;
    }

    if (type_name == STATICS.interned_image_color_2d)
    {
        type->class = COMPILER_INSTANCE_TYPE_CLASS_IMAGE;
        type->image_type = KAN_RPL_IMAGE_TYPE_COLOR_2D;
        return KAN_TRUE;
    }

    if (type_name == STATICS.interned_image_color_3d)
    {
        type->class = COMPILER_INSTANCE_TYPE_CLASS_IMAGE;
        type->image_type = KAN_RPL_IMAGE_TYPE_COLOR_3D;
        return KAN_TRUE;
    }

    if (type_name == STATICS.interned_image_color_cube)
    {
        type->class = COMPILER_INSTANCE_TYPE_CLASS_IMAGE;
        type->image_type = KAN_RPL_IMAGE_TYPE_COLOR_CUBE;
        return KAN_TRUE;
    }

    if (type_name == STATICS.interned_image_color_2d_array)
    {
        type->class = COMPILER_INSTANCE_TYPE_CLASS_IMAGE;
        type->image_type = KAN_RPL_IMAGE_TYPE_COLOR_2D_ARRAY;
        return KAN_TRUE;
    }

    if (type_name == STATICS.interned_image_depth_2d)
    {
        type->class = COMPILER_INSTANCE_TYPE_CLASS_IMAGE;
        type->image_type = KAN_RPL_IMAGE_TYPE_DEPTH_2D;
        return KAN_TRUE;
    }

    if (type_name == STATICS.interned_image_depth_3d)
    {
        type->class = COMPILER_INSTANCE_TYPE_CLASS_IMAGE;
        type->image_type = KAN_RPL_IMAGE_TYPE_DEPTH_3D;
        return KAN_TRUE;
    }

    if (type_name == STATICS.interned_image_depth_cube)
    {
        type->class = COMPILER_INSTANCE_TYPE_CLASS_IMAGE;
        type->image_type = KAN_RPL_IMAGE_TYPE_DEPTH_CUBE;
        return KAN_TRUE;
    }

    if (type_name == STATICS.interned_image_depth_2d_array)
    {
        type->class = COMPILER_INSTANCE_TYPE_CLASS_IMAGE;
        type->image_type = KAN_RPL_IMAGE_TYPE_DEPTH_2D_ARRAY;
        return KAN_TRUE;
    }

    if (resolve_use_struct (context, instance, type_name, &type->struct_data))
    {
        type->class = COMPILER_INSTANCE_TYPE_CLASS_STRUCT;
        return KAN_TRUE;
    }

    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Declaration \"%s\" type \"%s\" is unknown.",
             context->log_name, intermediate_log_name, source_name_for_logging, (long) source_line_for_logging,
             declaration_name_for_logging, type_name)
    return KAN_FALSE;
}

static kan_bool_t is_global_name_occupied (struct rpl_compiler_context_t *context,
                                           struct rpl_compiler_instance_t *instance,
                                           kan_interned_string_t name)
{
    if (name == STATICS.sample_function_name)
    {
        return KAN_TRUE;
    }

    struct compiler_instance_struct_node_t *struct_data = instance->first_struct;
    while (struct_data)
    {
        if (struct_data->name == name)
        {
            return KAN_TRUE;
        }

        struct_data = struct_data->next;
    }

    struct compiler_instance_container_node_t *container = instance->first_container;
    while (container)
    {
        if (container->name == name)
        {
            return KAN_TRUE;
        }

        container = container->next;
    }

    struct compiler_instance_buffer_node_t *buffer = instance->first_buffer;
    while (buffer)
    {
        if (buffer->name == name)
        {
            return KAN_TRUE;
        }

        buffer = buffer->next;
    }

    struct compiler_instance_sampler_node_t *sampler = instance->first_sampler;
    while (sampler)
    {
        if (sampler->name == name)
        {
            return KAN_TRUE;
        }

        sampler = sampler->next;
    }

    struct compiler_instance_image_node_t *image = instance->first_image;
    while (image)
    {
        if (image->name == name)
        {
            return KAN_TRUE;
        }

        image = image->next;
    }

    struct compiler_instance_function_node_t *function = instance->first_function;
    while (function)
    {
        if (function->name == name)
        {
            return KAN_TRUE;
        }

        function = function->next;
    }

    for (kan_loop_size_t option_value_index = 0u; option_value_index < context->option_values.size;
         ++option_value_index)
    {
        struct rpl_compiler_context_option_value_t *value =
            &((struct rpl_compiler_context_option_value_t *) context->option_values.data)[option_value_index];

        if (value->name == name)
        {
            return KAN_TRUE;
        }
    }

    return KAN_FALSE;
}

static const char *get_input_pack_class_name (enum kan_rpl_input_pack_class_t pack_class)
{
    switch (pack_class)
    {
    case KAN_RPL_INPUT_PACK_CLASS_DEFAULT:
        return "default";

    case KAN_RPL_INPUT_PACK_CLASS_FLOAT:
        return "float";

    case KAN_RPL_INPUT_PACK_CLASS_UNORM:
        return "unorm";

    case KAN_RPL_INPUT_PACK_CLASS_SNORM:
        return "snorm";

    case KAN_RPL_INPUT_PACK_CLASS_UINT:
        return "uint";

    case KAN_RPL_INPUT_PACK_CLASS_SINT:
        return "sint";
    }

    KAN_ASSERT (KAN_FALSE)
    return "unknown";
}

static inline enum kan_rpl_meta_attribute_item_format_t convert_inbuilt_type_item_to_default_item_format (
    enum inbuilt_type_item_t item)
{
    switch (item)
    {
    case INBUILT_TYPE_ITEM_FLOAT:
        return KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_32;

    case INBUILT_TYPE_ITEM_UNSIGNED:
        return KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_32;

    case INBUILT_TYPE_ITEM_SIGNED:
        return KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_32;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_32;
}

static inline kan_bool_t resolve_item_format (struct compiler_instance_type_definition_t *type_definition,
                                              enum kan_rpl_input_pack_class_t pack_class,
                                              kan_rpl_size_t pack_bits,
                                              enum kan_rpl_meta_attribute_item_format_t *output)
{
    switch (pack_class)
    {
    case KAN_RPL_INPUT_PACK_CLASS_DEFAULT:
        switch (type_definition->class)
        {
        case COMPILER_INSTANCE_TYPE_CLASS_VOID:
        case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
        case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
        case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
        case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
        case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
            KAN_ASSERT (KAN_FALSE)
            break;

        case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
            *output = convert_inbuilt_type_item_to_default_item_format (type_definition->vector_data->item);
            return KAN_TRUE;

        case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
            *output = convert_inbuilt_type_item_to_default_item_format (type_definition->matrix_data->item);
            return KAN_TRUE;
        }

        break;

    case KAN_RPL_INPUT_PACK_CLASS_FLOAT:
        switch (pack_bits)
        {
        case 16u:
            *output = KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_16;
            return KAN_TRUE;

        case 32u:
            *output = KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_32;
            return KAN_TRUE;
        }

        break;

    case KAN_RPL_INPUT_PACK_CLASS_UNORM:
        switch (pack_bits)
        {
        case 8u:
            *output = KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_8;
            return KAN_TRUE;

        case 16u:
            *output = KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_16;
            return KAN_TRUE;
        }

        break;

    case KAN_RPL_INPUT_PACK_CLASS_SNORM:
        switch (pack_bits)
        {
        case 8u:
            *output = KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_8;
            return KAN_TRUE;

        case 16u:
            *output = KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_16;
            return KAN_TRUE;
        }

        break;

    case KAN_RPL_INPUT_PACK_CLASS_UINT:
        switch (pack_bits)
        {
        case 8u:
            *output = KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_8;
            return KAN_TRUE;

        case 16u:
            *output = KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_16;
            return KAN_TRUE;

        case 32u:
            *output = KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_32;
            return KAN_TRUE;
        }

        break;

    case KAN_RPL_INPUT_PACK_CLASS_SINT:
        switch (pack_bits)
        {
        case 8u:
            *output = KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_8;
            return KAN_TRUE;

        case 16u:
            *output = KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_16;
            return KAN_TRUE;

        case 32u:
            *output = KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_32;
            return KAN_TRUE;
        }

        break;
    }

    return KAN_FALSE;
}

static inline void resolve_copy_meta (struct rpl_compiler_instance_t *instance,
                                      struct kan_rpl_intermediate_t *intermediate,
                                      kan_rpl_size_t meta_list_size,
                                      kan_rpl_size_t meta_list_index,
                                      kan_instance_size_t *output_meta_count,
                                      kan_interned_string_t **output_meta)
{
    *output_meta_count = meta_list_size;
    if (meta_list_size > 0u)
    {
        *output_meta = kan_stack_group_allocator_allocate (&instance->resolve_allocator,
                                                           sizeof (kan_interned_string_t) * meta_list_size,
                                                           _Alignof (kan_interned_string_t));

        memcpy (*output_meta, &((kan_interned_string_t *) intermediate->meta_lists_storage.data)[meta_list_index],
                sizeof (kan_interned_string_t) * meta_list_size);
    }
    else
    {
        *output_meta = NULL;
    }
}

static kan_bool_t resolve_container_fields (struct rpl_compiler_context_t *context,
                                            struct rpl_compiler_instance_t *instance,
                                            struct kan_rpl_intermediate_t *intermediate,
                                            struct kan_dynamic_array_t *container_field_array,
                                            kan_bool_t instance_options_allowed,
                                            struct compiler_instance_container_field_node_t **first_output)
{
    kan_bool_t result = KAN_TRUE;
    kan_instance_size_t current_offset = 0u;

    struct compiler_instance_container_field_node_t *first = NULL;
    struct compiler_instance_container_field_node_t *last = NULL;

    for (kan_loop_size_t container_field_index = 0u; container_field_index < container_field_array->size;
         ++container_field_index)
    {
        struct kan_rpl_container_field_t *source_container_field =
            &((struct kan_rpl_container_field_t *) container_field_array->data)[container_field_index];

        switch (evaluate_conditional (context, intermediate, source_container_field->conditional_index,
                                      instance_options_allowed))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            result = KAN_FALSE;
            break;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
        {
            struct compiler_instance_container_field_node_t *target_container_field =
                KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&instance->resolve_allocator,
                                                          struct compiler_instance_container_field_node_t);

            target_container_field->next = NULL;
            target_container_field->variable.name = source_container_field->name;
            // Actual access is decided during resolve depending on resolve stage.
            target_container_field->variable.type.access = KAN_RPL_ACCESS_CLASS_READ_WRITE;
            target_container_field->variable.type.flags = 0u;

            target_container_field->variable.type.array_size_runtime = KAN_FALSE;
            target_container_field->variable.type.array_dimensions_count = 0u;
            target_container_field->variable.type.array_dimensions = 0u;
            target_container_field->first_usage_stage = NULL;

            struct inbuilt_vector_type_t *vector_type = NULL;
            struct inbuilt_matrix_type_t *matrix_type = NULL;

            if ((vector_type = find_inbuilt_vector_type (source_container_field->type_name)))
            {
                target_container_field->variable.type.class = COMPILER_INSTANCE_TYPE_CLASS_VECTOR;
                target_container_field->variable.type.vector_data = vector_type;
            }
            else if ((matrix_type = find_inbuilt_matrix_type (source_container_field->type_name)))
            {
                target_container_field->variable.type.class = COMPILER_INSTANCE_TYPE_CLASS_MATRIX;
                target_container_field->variable.type.matrix_data = matrix_type;
            }
            else
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Container field \"%s\" has type \"%s\". Only vectors and matrices (and not "
                         "arrays) are supported for containers due to location limitations.",
                         context->log_name, intermediate->log_name, source_container_field->source_name,
                         (long) source_container_field->source_line, source_container_field->name,
                         source_container_field->type_name)
                result = KAN_FALSE;
            }

            if (result)
            {
                if (!resolve_item_format (&target_container_field->variable.type, source_container_field->pack_class,
                                          source_container_field->pack_class_bits,
                                          &target_container_field->input_item_format))
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Container field \"%s\" has pack type \"%s%u\". Only vectors and matrices "
                             "(and not "
                             "arrays) are supported for containers due to location limitations.",
                             context->log_name, intermediate->log_name, source_container_field->source_name,
                             (long) source_container_field->source_line, source_container_field->name,
                             get_input_pack_class_name (source_container_field->pack_class),
                             (unsigned) source_container_field->pack_class_bits)
                    result = KAN_FALSE;
                }
            }

            if (result)
            {
                target_container_field->size =
                    kan_rpl_meta_attribute_item_format_get_size (target_container_field->input_item_format);

                if (vector_type)
                {
                    target_container_field->size *= vector_type->items_count;
                }
                else if (matrix_type)
                {
                    target_container_field->size *= matrix_type->rows * matrix_type->columns;
                }

                // We expect attribute alignment to be equal to its structure item alignment.
                target_container_field->alignment =
                    kan_rpl_meta_attribute_item_format_get_alignment (target_container_field->input_item_format);

                if (target_container_field->size != 0u && target_container_field->alignment != 0u)
                {
                    current_offset =
                        (kan_instance_size_t) kan_apply_alignment (current_offset, target_container_field->alignment);
                    target_container_field->offset_if_input = current_offset;
                    current_offset += target_container_field->size;
                }
            }

            resolve_copy_meta (instance, intermediate, source_container_field->meta_list_size,
                               source_container_field->meta_list_index, &target_container_field->meta_count,
                               &target_container_field->meta);

            target_container_field->module_name = intermediate->log_name;
            target_container_field->source_name = source_container_field->source_name;
            target_container_field->source_line = source_container_field->source_line;

            if (last)
            {
                last->next = target_container_field;
                last = target_container_field;
            }
            else
            {
                first = target_container_field;
                last = target_container_field;
            }

            break;
        }

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            break;
        }
    }

    if (result)
    {
        *first_output = first;
    }

    return result;
}

static inline void calculate_size_and_alignment_from_container_fields (
    struct compiler_instance_container_field_node_t *container_field,
    kan_instance_size_t *size_output,
    kan_instance_size_t *alignment_output)
{
    *size_output = 0u;
    *alignment_output = 1u;

    while (container_field)
    {
        if (!container_field->variable.type.array_size_runtime)
        {
            *size_output = container_field->offset_if_input + container_field->size;
        }

        *alignment_output = KAN_MAX (*alignment_output, container_field->alignment);
        container_field = container_field->next;
    }

    *size_output = (kan_instance_size_t) kan_apply_alignment (*size_output, *alignment_output);
}

static inline void assign_container_field_locations (struct compiler_instance_container_field_node_t *first_field,
                                                     kan_rpl_size_t *location_counter)
{
    struct compiler_instance_container_field_node_t *field = first_field;
    while (field)
    {
        field->location = *location_counter;
        switch (field->variable.type.class)
        {
        case COMPILER_INSTANCE_TYPE_CLASS_VOID:
        case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
        case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
        case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
        case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
        case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
            KAN_ASSERT (KAN_FALSE)
            break;

        case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
            ++*location_counter;
            break;

        case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
            // Each matrix column occupies its location as location data size is limited by vector types on hardware.
            *location_counter += field->variable.type.matrix_data->columns;
            break;
        }

        field = field->next;
    }
}

static kan_bool_t resolve_containers (struct rpl_compiler_context_t *context,
                                      struct rpl_compiler_instance_t *instance,
                                      struct kan_rpl_intermediate_t *intermediate,
                                      struct binding_location_assignment_counter_t *assignment_counter)
{
    kan_bool_t result = KAN_TRUE;
    for (kan_loop_size_t container_index = 0u; container_index < intermediate->containers.size; ++container_index)
    {
        struct kan_rpl_container_t *source_container =
            &((struct kan_rpl_container_t *) intermediate->containers.data)[container_index];

        switch (evaluate_conditional (context, intermediate, source_container->conditional_index, KAN_FALSE))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            result = KAN_FALSE;
            break;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
        {
            if (is_global_name_occupied (context, instance, source_container->name))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Cannot resolve container \"%s\" as its global name is already occupied.",
                         context->log_name, intermediate->log_name, source_container->source_name,
                         (long) source_container->source_line, source_container->name)

                result = KAN_FALSE;
                break;
            }

            struct compiler_instance_container_node_t *target_container = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                &instance->resolve_allocator, struct compiler_instance_container_node_t);

            target_container->next = NULL;
            target_container->name = source_container->name;
            target_container->type = source_container->type;
            target_container->used = KAN_FALSE;

            kan_bool_t fields_affect_pipeline_input_interface = KAN_FALSE;
            switch (target_container->type)
            {
            case KAN_RPL_CONTAINER_TYPE_VERTEX_ATTRIBUTE:
            case KAN_RPL_CONTAINER_TYPE_INSTANCED_ATTRIBUTE:
                fields_affect_pipeline_input_interface = KAN_TRUE;
                break;

            case KAN_RPL_CONTAINER_TYPE_STATE:
            case KAN_RPL_CONTAINER_TYPE_COLOR_OUTPUT:
                fields_affect_pipeline_input_interface = KAN_FALSE;
                break;
            }

            if (!resolve_container_fields (context, instance, intermediate, &source_container->fields,
                                           !fields_affect_pipeline_input_interface, &target_container->first_field))
            {
                result = KAN_FALSE;
            }

            switch (target_container->type)
            {
            case KAN_RPL_CONTAINER_TYPE_VERTEX_ATTRIBUTE:
            case KAN_RPL_CONTAINER_TYPE_INSTANCED_ATTRIBUTE:
                target_container->binding_if_input = assignment_counter->next_attribute_container_binding;
                ++assignment_counter->next_attribute_container_binding;
                break;

            case KAN_RPL_CONTAINER_TYPE_STATE:
            case KAN_RPL_CONTAINER_TYPE_COLOR_OUTPUT:
                target_container->binding_if_input = INVALID_BINDING;
                break;
            }

            if (result)
            {
                switch (target_container->type)
                {
                case KAN_RPL_CONTAINER_TYPE_VERTEX_ATTRIBUTE:
                case KAN_RPL_CONTAINER_TYPE_INSTANCED_ATTRIBUTE:
                {
                    assign_container_field_locations (target_container->first_field,
                                                      &assignment_counter->next_attribute_location);
                    break;
                }

                case KAN_RPL_CONTAINER_TYPE_STATE:
                {
                    assign_container_field_locations (target_container->first_field,
                                                      &assignment_counter->next_state_location);
                    break;
                }

                case KAN_RPL_CONTAINER_TYPE_COLOR_OUTPUT:
                {
                    struct compiler_instance_container_field_node_t *field = target_container->first_field;
                    while (field)
                    {
                        if (field->variable.type.class != COMPILER_INSTANCE_TYPE_CLASS_VECTOR)
                        {
                            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                                     "[%s:%s:%s:%ld] Color output should only contain vector declarations, but field "
                                     "\"%s\" of container \"%s\" with other type found.",
                                     context->log_name, intermediate->log_name, field->source_name,
                                     (long) field->source_line, field->variable.name, target_container->name)
                            result = KAN_FALSE;
                        }

                        field = field->next;
                    }

                    assign_container_field_locations (target_container->first_field,
                                                      &assignment_counter->next_color_output_location);
                    break;
                }
                }
            }

            calculate_size_and_alignment_from_container_fields (target_container->first_field,
                                                                &target_container->block_size_if_input,
                                                                &target_container->block_alignment_if_input);

            target_container->module_name = intermediate->log_name;
            target_container->source_name = source_container->source_name;
            target_container->source_line = source_container->source_line;

            if (instance->last_container)
            {
                instance->last_container->next = target_container;
                instance->last_container = target_container;
            }
            else
            {
                instance->first_container = target_container;
                instance->last_container = target_container;
            }

            break;
        }

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            break;
        }
    }

    return result;
}

static kan_bool_t resolve_structure_field_declarations (struct rpl_compiler_context_t *context,
                                                        struct rpl_compiler_instance_t *instance,
                                                        struct kan_rpl_intermediate_t *intermediate,
                                                        struct kan_dynamic_array_t *declaration_array,
                                                        struct compiler_instance_declaration_node_t **first_output)
{
    kan_bool_t result = KAN_TRUE;
    kan_instance_size_t current_offset = 0u;

    struct compiler_instance_declaration_node_t *first = NULL;
    struct compiler_instance_declaration_node_t *last = NULL;

    for (kan_loop_size_t declaration_index = 0u; declaration_index < declaration_array->size; ++declaration_index)
    {
        struct kan_rpl_declaration_t *source_declaration =
            &((struct kan_rpl_declaration_t *) declaration_array->data)[declaration_index];

        switch (evaluate_conditional (context, intermediate, source_declaration->conditional_index, KAN_FALSE))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            result = KAN_FALSE;
            break;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
        {
            struct compiler_instance_declaration_node_t *target_declaration = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                &instance->resolve_allocator, struct compiler_instance_declaration_node_t);

            target_declaration->next = NULL;
            target_declaration->variable.name = source_declaration->name;
            target_declaration->variable.type.class = COMPILER_INSTANCE_TYPE_CLASS_VOID;

            if (!resolve_type (context, instance, intermediate->log_name, &target_declaration->variable.type,
                               source_declaration->type_name, source_declaration->name, source_declaration->source_name,
                               source_declaration->source_line))
            {
                result = KAN_FALSE;
            }

            if (!resolve_array_dimensions (context, instance, intermediate, &target_declaration->variable,
                                           source_declaration->array_size_runtime,
                                           source_declaration->array_size_expression_list_size,
                                           source_declaration->array_size_expression_list_index, KAN_FALSE))
            {
                result = KAN_FALSE;
            }

            if (result)
            {
                switch (target_declaration->variable.type.class)
                {
                case COMPILER_INSTANCE_TYPE_CLASS_VOID:
                case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
                case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
                case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
                case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Declaration \"%s\" has type \"%s\" which is not supported for fields.",
                             context->log_name, intermediate->log_name, source_declaration->source_name,
                             (long) source_declaration->source_line, source_declaration->name,
                             source_declaration->type_name)
                    result = KAN_FALSE;
                    break;

                case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
                case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
                case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
                    break;
                }
            }

            if (result)
            {
                calculate_type_definition_size_and_alignment (
                    &target_declaration->variable.type, 0u, &target_declaration->size, &target_declaration->alignment);

                if (target_declaration->size != 0u && target_declaration->alignment != 0u)
                {
                    current_offset =
                        (kan_instance_size_t) kan_apply_alignment (current_offset, target_declaration->alignment);
                    target_declaration->offset = current_offset;
                    current_offset += target_declaration->size;
                }
            }

            resolve_copy_meta (instance, intermediate, source_declaration->meta_list_size,
                               source_declaration->meta_list_index, &target_declaration->meta_count,
                               &target_declaration->meta);

            target_declaration->module_name = intermediate->log_name;
            target_declaration->source_name = source_declaration->source_name;
            target_declaration->source_line = source_declaration->source_line;

            if (last)
            {
                last->next = target_declaration;
                last = target_declaration;
            }
            else
            {
                first = target_declaration;
                last = target_declaration;
            }

            break;
        }

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            break;
        }
    }

    if (result)
    {
        *first_output = first;
    }

    return result;
}

static inline void calculate_size_and_alignment_from_declarations (
    struct compiler_instance_declaration_node_t *declaration,
    kan_instance_size_t *size_output,
    kan_instance_size_t *alignment_output)
{
    *size_output = 0u;
    *alignment_output = 1u;

    while (declaration)
    {
        if (!declaration->variable.type.array_size_runtime)
        {
            *size_output = declaration->offset + declaration->size;
        }

        *alignment_output = KAN_MAX (*alignment_output, declaration->alignment);
        declaration = declaration->next;
    }

    *size_output = (kan_instance_size_t) kan_apply_alignment (*size_output, *alignment_output);
}

static kan_bool_t resolve_buffers_validate_uniform_internals_alignment (
    struct rpl_compiler_context_t *context,
    struct compiler_instance_buffer_node_t *buffer,
    struct compiler_instance_declaration_node_t *first_declaration)
{
    kan_bool_t valid = KAN_TRUE;
    struct compiler_instance_declaration_node_t *declaration = first_declaration;

    while (declaration)
    {
        switch (declaration->variable.type.class)
        {
        case COMPILER_INSTANCE_TYPE_CLASS_VOID:
        case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
        case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
        case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
        case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
            // Cannot be fields, should be detected and aborted earlier.
            KAN_ASSERT (KAN_FALSE)
            break;

        case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
        {
            const kan_instance_size_t size = declaration->variable.type.vector_data->items_count *
                                             inbuilt_type_item_size[declaration->variable.type.vector_data->item];

            if (size % 16u != 0u)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Declaration \"%s\" is found inside buffer \"%s\", but its size is not "
                         "multiple of 16, which is prone to cause errors when used with uniform buffers.",
                         context->log_name, declaration->module_name, declaration->source_name,
                         (long) declaration->source_line, declaration->variable.name, buffer->name)
                valid = KAN_FALSE;
            }

            break;
        }

        case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
        {
            const kan_instance_size_t size = declaration->variable.type.matrix_data->rows *
                                             declaration->variable.type.matrix_data->columns *
                                             inbuilt_type_item_size[declaration->variable.type.matrix_data->item];

            if (size % 16u != 0u)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Declaration \"%s\" is found inside buffer \"%s\", but its size is not "
                         "multiple of 16, which is prone to cause errors when used with uniform buffers.",
                         context->log_name, declaration->module_name, declaration->source_name,
                         (long) declaration->source_line, declaration->variable.name, buffer->name)
                valid = KAN_FALSE;
            }

            break;
        }

        case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
            if (!resolve_buffers_validate_uniform_internals_alignment (
                    context, buffer, declaration->variable.type.struct_data->first_field))
            {
                valid = KAN_FALSE;
            }

            break;
        }

        declaration = declaration->next;
    }

    return valid;
}

static kan_bool_t resolve_buffers_validate_buffer_tail_if_any (
    struct rpl_compiler_context_t *context,
    struct compiler_instance_buffer_node_t *buffer,
    struct compiler_instance_declaration_node_t *first_declaration,
    kan_bool_t *has_tail_output)
{
    kan_bool_t valid = KAN_TRUE;
    struct compiler_instance_declaration_node_t *declaration = first_declaration;

    while (declaration)
    {
        if (declaration->variable.type.array_size_runtime)
        {
            buffer->tail_item_size = declaration->size;
            *has_tail_output = KAN_TRUE;
            valid = !declaration->next;

            if (!valid)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Runtime size array declaration \"%s\" is found inside buffer \"%s\", but its "
                         "not the last field.",
                         context->log_name, declaration->module_name, declaration->source_name,
                         (long) declaration->source_line, declaration->variable.name, buffer->name)
            }
        }
        else if (declaration->variable.type.class == COMPILER_INSTANCE_TYPE_CLASS_STRUCT)
        {
            if (!resolve_buffers_validate_buffer_tail_if_any (
                    context, buffer, declaration->variable.type.struct_data->first_field, has_tail_output))
            {
                valid = KAN_FALSE;
            }

            if (*has_tail_output && declaration->next)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Runtime size array found inside field \"%s\" declaration inside buffer "
                         "\"%s\", but its not the last field.",
                         context->log_name, declaration->module_name, declaration->source_name,
                         (long) declaration->source_line, declaration->variable.name, buffer->name)
                valid = KAN_FALSE;
            }
        }

        declaration = declaration->next;
    }

    return valid;
}

static kan_bool_t resolve_buffers_of_type (struct rpl_compiler_context_t *context,
                                           struct rpl_compiler_instance_t *instance,
                                           struct kan_rpl_intermediate_t *intermediate,
                                           struct binding_location_assignment_counter_t *assignment_counter,
                                           enum kan_rpl_buffer_type_t buffer_type)
{
    kan_bool_t result = KAN_TRUE;
    for (kan_loop_size_t buffer_index = 0u; buffer_index < intermediate->buffers.size; ++buffer_index)
    {
        struct kan_rpl_buffer_t *source_buffer =
            &((struct kan_rpl_buffer_t *) intermediate->buffers.data)[buffer_index];

        // Buffer of other type, will be resolved later.
        if (source_buffer->type != buffer_type)
        {
            continue;
        }

        switch (evaluate_conditional (context, intermediate, source_buffer->conditional_index, KAN_FALSE))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            result = KAN_FALSE;
            break;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
        {
            if (is_global_name_occupied (context, instance, source_buffer->name))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Cannot resolve buffer \"%s\" as its global name is already occupied.",
                         context->log_name, intermediate->log_name, source_buffer->source_name,
                         (long) source_buffer->source_line, source_buffer->name)

                result = KAN_FALSE;
                break;
            }

            struct compiler_instance_buffer_node_t *target_buffer = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                &instance->resolve_allocator, struct compiler_instance_buffer_node_t);

            target_buffer->next = NULL;
            target_buffer->name = source_buffer->name;
            target_buffer->set = source_buffer->set;
            target_buffer->type = source_buffer->type;
            target_buffer->used = KAN_FALSE;

            if (!resolve_structure_field_declarations (context, instance, intermediate, &source_buffer->fields,
                                                       &target_buffer->first_field))
            {
                result = KAN_FALSE;
            }

            switch (target_buffer->type)
            {
            case KAN_RPL_BUFFER_TYPE_UNIFORM:
            case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
                switch (target_buffer->set)
                {
                case KAN_RPL_SET_PASS:
                    target_buffer->binding = assignment_counter->next_pass_set_binding++;
                    break;

                case KAN_RPL_SET_MATERIAL:
                    target_buffer->binding = assignment_counter->next_material_set_binding++;
                    break;

                case KAN_RPL_SET_OBJECT:
                    target_buffer->binding = assignment_counter->next_object_set_binding++;
                    break;

                case KAN_RPL_SET_SHARED:
                    target_buffer->binding = assignment_counter->next_shared_set_binding++;
                    break;
                }

                break;
            }

            target_buffer->tail_item_size = 0u;
            if (result)
            {
                switch (target_buffer->type)
                {
                case KAN_RPL_BUFFER_TYPE_UNIFORM:
                {
                    if (!resolve_buffers_validate_uniform_internals_alignment (context, target_buffer,
                                                                               target_buffer->first_field))
                    {
                        result = KAN_FALSE;
                    }

                    kan_bool_t has_tail = KAN_FALSE;
                    if (!resolve_buffers_validate_buffer_tail_if_any (context, target_buffer,
                                                                      target_buffer->first_field, &has_tail))
                    {
                        result = KAN_FALSE;
                    }

                    if (has_tail)
                    {
                        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                                 "[%s:%s:%s:%ld] Uniform buffer cannot have tails, but buffer \"%s\" has one.",
                                 context->log_name, intermediate->log_name, target_buffer->source_name,
                                 (long) target_buffer->source_line, target_buffer->name)
                        result = KAN_FALSE;
                    }

                    break;
                }

                case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
                {
                    kan_bool_t has_tail = KAN_FALSE;
                    if (!resolve_buffers_validate_buffer_tail_if_any (context, target_buffer,
                                                                      target_buffer->first_field, &has_tail))
                    {
                        result = KAN_FALSE;
                    }

                    break;
                }
                }
            }

            calculate_size_and_alignment_from_declarations (target_buffer->first_field, &target_buffer->main_size,
                                                            &target_buffer->alignment);

            target_buffer->module_name = intermediate->log_name;
            target_buffer->source_name = source_buffer->source_name;
            target_buffer->source_line = source_buffer->source_line;

            if (instance->last_buffer)
            {
                instance->last_buffer->next = target_buffer;
                instance->last_buffer = target_buffer;
            }
            else
            {
                instance->first_buffer = target_buffer;
                instance->last_buffer = target_buffer;
            }

            break;
        }

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            break;
        }
    }

    return result;
}

static kan_bool_t resolve_buffers (struct rpl_compiler_context_t *context,
                                   struct rpl_compiler_instance_t *instance,
                                   struct kan_rpl_intermediate_t *intermediate,
                                   struct binding_location_assignment_counter_t *assignment_counter)
{
    // Buffer resolution is ordered by buffer types in order to make resulting pipeline bindings layouts more common.
    return resolve_buffers_of_type (context, instance, intermediate, assignment_counter, KAN_RPL_BUFFER_TYPE_UNIFORM) &
           resolve_buffers_of_type (context, instance, intermediate, assignment_counter,
                                    KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE);
}

static kan_bool_t resolve_samplers (struct rpl_compiler_context_t *context,
                                    struct rpl_compiler_instance_t *instance,
                                    struct kan_rpl_intermediate_t *intermediate,
                                    struct binding_location_assignment_counter_t *assignment_counter)
{
    kan_bool_t result = KAN_TRUE;
    for (kan_loop_size_t sampler_index = 0u; sampler_index < intermediate->samplers.size; ++sampler_index)
    {
        struct kan_rpl_sampler_t *source_sampler =
            &((struct kan_rpl_sampler_t *) intermediate->samplers.data)[sampler_index];

        switch (evaluate_conditional (context, intermediate, source_sampler->conditional_index, KAN_FALSE))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            result = KAN_FALSE;
            break;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
        {
            if (is_global_name_occupied (context, instance, source_sampler->name))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Cannot resolve sampler \"%s\" as its global name is already occupied.",
                         context->log_name, intermediate->log_name, source_sampler->source_name,
                         (long) source_sampler->source_line, source_sampler->name)

                result = KAN_FALSE;
                break;
            }

            struct compiler_instance_sampler_node_t *target_sampler = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                &instance->resolve_allocator, struct compiler_instance_sampler_node_t);

            target_sampler->next = NULL;
            target_sampler->name = source_sampler->name;

            target_sampler->used = KAN_FALSE;
            target_sampler->set = source_sampler->set;

            switch (target_sampler->set)
            {
            case KAN_RPL_SET_PASS:
                target_sampler->binding = assignment_counter->next_pass_set_binding++;
                break;

            case KAN_RPL_SET_MATERIAL:
                target_sampler->binding = assignment_counter->next_material_set_binding++;
                break;

            case KAN_RPL_SET_OBJECT:
                target_sampler->binding = assignment_counter->next_object_set_binding++;
                break;

            case KAN_RPL_SET_SHARED:
                target_sampler->binding = assignment_counter->next_shared_set_binding++;
                break;
            }

            target_sampler->module_name = intermediate->log_name;
            target_sampler->source_name = source_sampler->source_name;
            target_sampler->source_line = source_sampler->source_line;

            if (instance->last_sampler)
            {
                instance->last_sampler->next = target_sampler;
                instance->last_sampler = target_sampler;
            }
            else
            {
                instance->first_sampler = target_sampler;
                instance->last_sampler = target_sampler;
            }

            break;
        }

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            break;
        }
    }

    return result;
}

static kan_bool_t resolve_images_of_type (struct rpl_compiler_context_t *context,
                                          struct rpl_compiler_instance_t *instance,
                                          struct kan_rpl_intermediate_t *intermediate,
                                          struct binding_location_assignment_counter_t *assignment_counter,
                                          enum kan_rpl_image_type_t image_type)
{
    kan_bool_t result = KAN_TRUE;
    for (kan_loop_size_t image_index = 0u; image_index < intermediate->images.size; ++image_index)
    {
        struct kan_rpl_image_t *source_image = &((struct kan_rpl_image_t *) intermediate->images.data)[image_index];

        // Image of other type, will be resolved later.
        if (source_image->type != image_type)
        {
            continue;
        }

        switch (evaluate_conditional (context, intermediate, source_image->conditional_index, KAN_FALSE))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            result = KAN_FALSE;
            break;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
        {
            if (is_global_name_occupied (context, instance, source_image->name))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Cannot resolve image \"%s\" as its global name is already occupied.",
                         context->log_name, intermediate->log_name, source_image->source_name,
                         (long) source_image->source_line, source_image->name)

                result = KAN_FALSE;
                break;
            }

            struct compiler_instance_image_node_t *target_image = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                &instance->resolve_allocator, struct compiler_instance_image_node_t);

            target_image->next = NULL;
            target_image->name = source_image->name;
            target_image->set = source_image->set;
            target_image->type = source_image->type;
            target_image->used = KAN_FALSE;
            target_image->array_size = 1u;

            if (source_image->array_size_index != KAN_RPL_EXPRESSION_INDEX_NONE)
            {
                if (!resolve_array_dimension_value (context, instance, intermediate, source_image->array_size_index,
                                                    &target_image->array_size, KAN_FALSE, 0u))
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Cannot resolve image \"%s\" due to error while array size resolution.",
                             context->log_name, intermediate->log_name, source_image->source_name,
                             (long) source_image->source_line, source_image->name)

                    result = KAN_FALSE;
                    break;
                }
            }

            switch (target_image->set)
            {
            case KAN_RPL_SET_PASS:
                target_image->binding = assignment_counter->next_pass_set_binding++;
                break;

            case KAN_RPL_SET_MATERIAL:
                target_image->binding = assignment_counter->next_material_set_binding++;
                break;

            case KAN_RPL_SET_OBJECT:
                target_image->binding = assignment_counter->next_object_set_binding++;
                break;

            case KAN_RPL_SET_SHARED:
                target_image->binding = assignment_counter->next_shared_set_binding++;
                break;
            }

            target_image->module_name = intermediate->log_name;
            target_image->source_name = source_image->source_name;
            target_image->source_line = source_image->source_line;

            if (instance->last_image)
            {
                instance->last_image->next = target_image;
                instance->last_image = target_image;
            }
            else
            {
                instance->first_image = target_image;
                instance->last_image = target_image;
            }

            break;
        }

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            break;
        }
    }

    return result;
}

static kan_bool_t resolve_images (struct rpl_compiler_context_t *context,
                                  struct rpl_compiler_instance_t *instance,
                                  struct kan_rpl_intermediate_t *intermediate,
                                  struct binding_location_assignment_counter_t *assignment_counter)
{
    // Image resolution is ordered by image types in order to make resulting pipeline bindings layouts more common.
    return resolve_images_of_type (context, instance, intermediate, assignment_counter, KAN_RPL_IMAGE_TYPE_COLOR_2D) &
           resolve_images_of_type (context, instance, intermediate, assignment_counter, KAN_RPL_IMAGE_TYPE_COLOR_3D) &
           resolve_images_of_type (context, instance, intermediate, assignment_counter, KAN_RPL_IMAGE_TYPE_COLOR_CUBE) &
           resolve_images_of_type (context, instance, intermediate, assignment_counter,
                                   KAN_RPL_IMAGE_TYPE_COLOR_2D_ARRAY) &
           resolve_images_of_type (context, instance, intermediate, assignment_counter, KAN_RPL_IMAGE_TYPE_DEPTH_2D) &
           resolve_images_of_type (context, instance, intermediate, assignment_counter, KAN_RPL_IMAGE_TYPE_DEPTH_3D) &
           resolve_images_of_type (context, instance, intermediate, assignment_counter, KAN_RPL_IMAGE_TYPE_DEPTH_CUBE) &
           resolve_images_of_type (context, instance, intermediate, assignment_counter,
                                   KAN_RPL_IMAGE_TYPE_DEPTH_2D_ARRAY);
}

static const char *get_stage_name (enum kan_rpl_pipeline_stage_t stage)
{
    switch (stage)
    {
    case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX:
        return "classic_pipeline_vertex_stage";
    case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT:
        return "classic_pipeline_fragment_stage";
    }

    return "unknown_pipeline_stage";
}

static kan_bool_t check_alias_or_variable_name_is_not_occupied (struct rpl_compiler_context_t *context,
                                                                struct rpl_compiler_instance_t *instance,
                                                                struct resolve_expression_scope_t *resolve_scope,
                                                                kan_interned_string_t name)
{
    struct resolve_expression_alias_node_t *alias_node = resolve_scope->first_alias;
    while (alias_node)
    {
        if (alias_node->name == name)
        {
            return KAN_FALSE;
        }

        alias_node = alias_node->next;
    }

    if (resolve_scope->associated_resolved_scope_if_any)
    {
        struct compiler_instance_scope_variable_item_t *variable =
            resolve_scope->associated_resolved_scope_if_any->scope.first_variable;

        while (variable)
        {
            if (variable->variable->name == name)
            {
                return KAN_FALSE;
            }

            variable = variable->next;
        }
    }

    if (resolve_scope->parent)
    {
        return check_alias_or_variable_name_is_not_occupied (context, instance, resolve_scope->parent, name);
    }

    return !is_global_name_occupied (context, instance, name);
}

static struct resolve_expression_alias_node_t *resolve_find_alias (struct resolve_expression_scope_t *resolve_scope,
                                                                   kan_interned_string_t identifier)
{
    struct resolve_expression_alias_node_t *alias_node = resolve_scope->first_alias;
    while (alias_node)
    {
        if (alias_node->name == identifier)
        {
            return alias_node;
        }

        alias_node = alias_node->next;
    }

    if (resolve_scope->parent)
    {
        return resolve_find_alias (resolve_scope->parent, identifier);
    }

    return NULL;
}

static kan_bool_t resolve_use_struct (struct rpl_compiler_context_t *context,
                                      struct rpl_compiler_instance_t *instance,
                                      kan_interned_string_t name,
                                      struct compiler_instance_struct_node_t **output)
{
    *output = NULL;
    struct compiler_instance_struct_node_t *struct_node = instance->first_struct;

    while (struct_node)
    {
        if (struct_node->name == name)
        {
            *output = struct_node;
            // Already resolved.
            return KAN_TRUE;
        }

        struct_node = struct_node->next;
    }

    kan_bool_t resolve_successful = KAN_TRUE;
    struct kan_rpl_struct_t *intermediate_struct = NULL;
    struct kan_rpl_intermediate_t *selected_intermediate = NULL;

    for (kan_loop_size_t intermediate_index = 0u; intermediate_index < context->modules.size; ++intermediate_index)
    {
        struct kan_rpl_intermediate_t *intermediate =
            ((struct kan_rpl_intermediate_t **) context->modules.data)[intermediate_index];

        for (kan_loop_size_t struct_index = 0u; struct_index < intermediate->structs.size; ++struct_index)
        {
            struct kan_rpl_struct_t *struct_data =
                &((struct kan_rpl_struct_t *) intermediate->structs.data)[struct_index];

            if (struct_data->name == name)
            {
                switch (evaluate_conditional (context, intermediate, struct_data->conditional_index, KAN_FALSE))
                {
                case CONDITIONAL_EVALUATION_RESULT_FAILED:
                    resolve_successful = KAN_FALSE;
                    break;

                case CONDITIONAL_EVALUATION_RESULT_TRUE:
                    if (intermediate_struct)
                    {
                        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                                 "[%s:%s:%s:%ld] Encountered duplicate active definition of struct \"%s\".",
                                 context->log_name, intermediate->log_name, struct_data->source_name,
                                 (long) struct_data->source_line, struct_data->name)
                        resolve_successful = KAN_FALSE;
                    }

                    intermediate_struct = struct_data;
                    selected_intermediate = intermediate;
                    break;

                case CONDITIONAL_EVALUATION_RESULT_FALSE:
                    break;
                }
            }
        }
    }

    if (!resolve_successful)
    {
        return KAN_FALSE;
    }

    if (!intermediate_struct)
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s] Unable to find struct \"%s\".", context->log_name, name)
        return KAN_FALSE;
    }

    struct_node =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&instance->resolve_allocator, struct compiler_instance_struct_node_t);
    *output = struct_node;

    struct_node->name = name;
    struct_node->module_name = selected_intermediate->log_name;
    struct_node->source_name = intermediate_struct->source_name;
    struct_node->source_line = intermediate_struct->source_line;

    if (!resolve_structure_field_declarations (context, instance, selected_intermediate, &intermediate_struct->fields,
                                               &struct_node->first_field))
    {
        resolve_successful = KAN_FALSE;
    }

    calculate_size_and_alignment_from_declarations (struct_node->first_field, &struct_node->size,
                                                    &struct_node->alignment);
    struct_node->next = NULL;

    if (instance->last_struct)
    {
        instance->last_struct->next = struct_node;
    }
    else
    {
        instance->first_struct = struct_node;
    }

    instance->last_struct = struct_node;
    return resolve_successful;
}

static kan_bool_t is_container_can_be_accessed_from_stage (struct compiler_instance_container_node_t *container,
                                                           enum kan_rpl_pipeline_stage_t stage)
{
    switch (container->type)
    {
    case KAN_RPL_CONTAINER_TYPE_VERTEX_ATTRIBUTE:
    case KAN_RPL_CONTAINER_TYPE_INSTANCED_ATTRIBUTE:
        return stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX;

    case KAN_RPL_CONTAINER_TYPE_STATE:
        return KAN_TRUE;

    case KAN_RPL_CONTAINER_TYPE_COLOR_OUTPUT:
        return stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_TRUE;
}

static inline kan_bool_t resolve_bind_function_to_stage (struct rpl_compiler_context_t *context,
                                                         struct compiler_instance_function_node_t *function,
                                                         enum kan_rpl_pipeline_stage_t stage,
                                                         kan_rpl_size_t usage_line,
                                                         kan_interned_string_t global_name)
{
    if (function->has_stage_specific_access)
    {
        if (function->required_stage != stage)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Function \"%s\" is already bound to stage \"%s\" and cannot be accessed from "
                     "other stages due to its buffer accesses, but it also tries to access global (buffer or sampler) "
                     "\"%s\" that wants to bind function to stage \"%s\".",
                     context->log_name, function->module_name, function->source_name, (long) usage_line, function->name,
                     get_stage_name (function->required_stage), global_name, get_stage_name (stage))
            return KAN_FALSE;
        }
    }
    else
    {
        function->has_stage_specific_access = KAN_TRUE;
        function->required_stage = stage;
    }

    return KAN_TRUE;
}

static kan_bool_t resolve_use_container (struct rpl_compiler_context_t *context,
                                         struct rpl_compiler_instance_t *instance,
                                         struct compiler_instance_function_node_t *function,
                                         enum kan_rpl_pipeline_stage_t stage,
                                         struct compiler_instance_container_node_t *container,
                                         kan_rpl_size_t usage_line)
{
    struct compiler_instance_container_access_node_t *access_node = function->first_container_access;
    while (access_node)
    {
        if (access_node->container == container)
        {
            // Already used, no need for further verification.
            return KAN_TRUE;
        }

        access_node = access_node->next;
    }

    access_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&instance->resolve_allocator,
                                                            struct compiler_instance_container_access_node_t);

    access_node->next = function->first_container_access;
    function->first_container_access = access_node;
    access_node->container = container;
    access_node->direct_access_function = function;

    if (!is_container_can_be_accessed_from_stage (container, stage))
    {
        KAN_LOG (
            rpl_compiler_context, KAN_LOG_ERROR,
            "[%s:%s:%s:%ld] Function \"%s\" is called in stage \"%s\" and tries to access container \"%s\" which is "
            "not accessible in that stage.",
            context->log_name, function->module_name, function->source_name, (long) usage_line, function->name,
            get_stage_name (stage), container->name)
        return KAN_FALSE;
    }

    // Container access is always stage specific.
    if (!resolve_bind_function_to_stage (context, function, stage, usage_line, container->name))
    {
        return KAN_FALSE;
    }

    container->used = KAN_TRUE;
    return KAN_TRUE;
}

static kan_bool_t resolve_use_buffer (struct rpl_compiler_context_t *context,
                                      struct rpl_compiler_instance_t *instance,
                                      struct compiler_instance_function_node_t *function,
                                      enum kan_rpl_pipeline_stage_t stage,
                                      struct compiler_instance_buffer_node_t *buffer,
                                      kan_rpl_size_t usage_line)
{
    struct compiler_instance_buffer_access_node_t *access_node = function->first_buffer_access;
    while (access_node)
    {
        if (access_node->buffer == buffer)
        {
            // Already used, no need for further verification.
            return KAN_TRUE;
        }

        access_node = access_node->next;
    }

    access_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&instance->resolve_allocator,
                                                            struct compiler_instance_buffer_access_node_t);

    access_node->next = function->first_buffer_access;
    function->first_buffer_access = access_node;
    access_node->buffer = buffer;
    access_node->direct_access_function = function;
    // Buffer access is not stage specific, therefore there is no additional stage binding.
    buffer->used = KAN_TRUE;
    return KAN_TRUE;
}

static kan_bool_t resolve_use_sampler (struct rpl_compiler_context_t *context,
                                       struct rpl_compiler_instance_t *instance,
                                       struct compiler_instance_function_node_t *function,
                                       struct compiler_instance_sampler_node_t *sampler,
                                       kan_rpl_size_t usage_line)
{
    struct compiler_instance_sampler_access_node_t *access_node = function->first_sampler_access;
    while (access_node)
    {
        if (access_node->sampler == sampler)
        {
            // Already used, no need fop further verification.
            return KAN_TRUE;
        }

        access_node = access_node->next;
    }

    access_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&instance->resolve_allocator,
                                                            struct compiler_instance_sampler_access_node_t);

    access_node->next = function->first_sampler_access;
    function->first_sampler_access = access_node;
    access_node->sampler = sampler;
    access_node->direct_access_function = function;
    // Sampling is not stage specific, therefore there is no additional stage binding.
    sampler->used = KAN_TRUE;
    return KAN_TRUE;
}

static kan_bool_t resolve_use_image (struct rpl_compiler_context_t *context,
                                     struct rpl_compiler_instance_t *instance,
                                     struct compiler_instance_function_node_t *function,
                                     struct compiler_instance_image_node_t *image,
                                     kan_rpl_size_t usage_line)
{
    struct compiler_instance_image_access_node_t *access_node = function->first_image_access;
    while (access_node)
    {
        if (access_node->image == image)
        {
            // Already used, no need fop further verification.
            return KAN_TRUE;
        }

        access_node = access_node->next;
    }

    access_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&instance->resolve_allocator,
                                                            struct compiler_instance_image_access_node_t);

    access_node->next = function->first_image_access;
    function->first_image_access = access_node;
    access_node->image = image;
    access_node->direct_access_function = function;
    // Image usage not stage specific, therefore there is no additional stage binding.
    image->used = KAN_TRUE;
    return KAN_TRUE;
}

static struct compiler_instance_scope_variable_item_t *resolve_find_variable (
    struct resolve_expression_scope_t *resolve_scope, kan_interned_string_t variable_name)
{
    if (resolve_scope->associated_resolved_scope_if_any)
    {
        struct compiler_instance_scope_variable_item_t *variable =
            resolve_scope->associated_resolved_scope_if_any->scope.first_variable;

        while (variable)
        {
            if (variable->variable->name == variable_name)
            {
                return variable;
            }

            variable = variable->next;
        }
    }

    if (resolve_scope->parent)
    {
        return resolve_find_variable (resolve_scope->parent, variable_name);
    }

    struct compiler_instance_scope_variable_item_t *variable = resolve_scope->function->first_argument_variable;
    while (variable)
    {
        if (variable->variable->name == variable_name)
        {
            return variable;
        }

        variable = variable->next;
    }

    return NULL;
}

static kan_bool_t resolve_expression (struct rpl_compiler_context_t *context,
                                      struct rpl_compiler_instance_t *instance,
                                      struct kan_rpl_intermediate_t *intermediate,
                                      struct resolve_expression_scope_t *resolve_scope,
                                      struct kan_rpl_expression_t *expression,
                                      struct compiler_instance_expression_node_t **output);

static const char *get_expression_call_name_for_logging (struct compiler_instance_expression_node_t *owner_expression)
{
    if (owner_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_FUNCTION_CALL)
    {
        return owner_expression->function_call.function->name;
    }
    else if (owner_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_SAMPLE)
    {
        return STATICS.sample_function_name;
    }
    else if (owner_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_VECTOR_CONSTRUCTOR)
    {
        return owner_expression->vector_constructor.type->name;
    }
    else if (owner_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_MATRIX_CONSTRUCTOR)
    {
        return owner_expression->matrix_constructor.type->name;
    }
    else if (owner_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCT_CONSTRUCTOR)
    {
        return owner_expression->struct_constructor.type->name;
    }

    return "<unknown_call_name>";
}

static inline kan_bool_t is_access_readable (enum kan_rpl_access_class_t access)
{
    switch (access)
    {
    case KAN_RPL_ACCESS_CLASS_READ_ONLY:
    case KAN_RPL_ACCESS_CLASS_READ_WRITE:
        return KAN_TRUE;

    case KAN_RPL_ACCESS_CLASS_WRITE_ONLY:
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t is_access_writeable (enum kan_rpl_access_class_t access)
{
    switch (access)
    {
    case KAN_RPL_ACCESS_CLASS_READ_ONLY:
        return KAN_FALSE;

    case KAN_RPL_ACCESS_CLASS_WRITE_ONLY:
    case KAN_RPL_ACCESS_CLASS_READ_WRITE:
        return KAN_TRUE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t resolve_match_signature_at_index (struct rpl_compiler_context_t *context,
                                                           kan_interned_string_t module_name,
                                                           struct compiler_instance_expression_node_t *owner_expression,
                                                           struct compiler_instance_type_definition_t *signature,
                                                           kan_rpl_size_t signature_index,
                                                           struct compiler_instance_expression_node_t *expression)
{
    if (signature)
    {
        if (!is_type_definition_base_equal (signature, &expression->output))
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Expression array item at index %ld for \"%s\" call has incorrect "
                     "type: \"%s\" while \"%s\" is expected.",
                     context->log_name, module_name, owner_expression->source_name,
                     (long) owner_expression->source_line, (long) signature_index,
                     get_expression_call_name_for_logging (owner_expression),
                     get_type_name_for_logging (&expression->output), get_type_name_for_logging (signature))
            return KAN_FALSE;
        }

        if (is_access_readable (signature->access) && !is_access_readable (expression->output.access))
        {
            KAN_LOG (
                rpl_compiler_context, KAN_LOG_ERROR,
                "[%s:%s:%s:%ld] Expression array item at index %ld for \"%s\" call must be readable, but it isn't.",
                context->log_name, module_name, owner_expression->source_name, (long) owner_expression->source_line,
                (long) signature_index, get_expression_call_name_for_logging (owner_expression))
            return KAN_FALSE;
        }

        if (is_access_writeable (signature->access) && !is_access_writeable (expression->output.access))
        {
            KAN_LOG (
                rpl_compiler_context, KAN_LOG_ERROR,
                "[%s:%s:%s:%ld] Expression array item at index %ld for \"%s\" call must be writeable, but it isn't.",
                context->log_name, module_name, owner_expression->source_name, (long) owner_expression->source_line,
                (long) signature_index, get_expression_call_name_for_logging (owner_expression))
            return KAN_FALSE;
        }

        if (is_access_writeable (signature->access) &&
            expression->output.flags & COMPILER_INSTANCE_TYPE_INTERFACE_POINTER)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Expression array item at index %ld for \"%s\" call must be writeable, but "
                     "argument is non-function-scope pointer. Due to logical pointer model limitations, only variables "
                     "declared in functions or to other arguments are allowed as writeable arguments.",
                     context->log_name, module_name, owner_expression->source_name,
                     (long) owner_expression->source_line, (long) signature_index,
                     get_expression_call_name_for_logging (owner_expression))
            return KAN_FALSE;
        }

        // Runtime arrays should not matter here as they should be disallowed on parser level.
        KAN_ASSERT (!signature->array_size_runtime)

        if (expression->output.array_size_runtime)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Expression array item at index %ld for \"%s\" call is a runtime array, which is "
                     "not allowed.",
                     context->log_name, module_name, owner_expression->source_name,
                     (long) owner_expression->source_line, (long) signature_index,
                     get_expression_call_name_for_logging (owner_expression))
            return KAN_FALSE;
        }

        if (signature->array_dimensions_count != expression->output.array_dimensions_count)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Expression array item at index %ld for \"%s\" call has incorrect "
                     "array dimension count: %ld while %ld is expected",
                     context->log_name, module_name, owner_expression->source_name,
                     (long) owner_expression->source_line, (long) signature_index,
                     get_expression_call_name_for_logging (owner_expression),
                     (long) expression->output.array_dimensions_count, (long) signature->array_dimensions_count)
            return KAN_FALSE;
        }

        for (kan_loop_size_t array_dimension_index = 0u; array_dimension_index < signature->array_dimensions_count;
             ++array_dimension_index)
        {
            if (signature->array_dimensions[array_dimension_index] !=
                expression->output.array_dimensions[array_dimension_index])
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Expression array item at index %ld for \"%s\" call has "
                         "incorrect array dimension %ld size: %ld while %ld is expected",
                         context->log_name, module_name, owner_expression->source_name,
                         (long) owner_expression->source_line, (long) signature_index,
                         get_expression_call_name_for_logging (owner_expression), (long) array_dimension_index,
                         (long) expression->output.array_dimensions[array_dimension_index],
                         (long) signature->array_dimensions[array_dimension_index])
                return KAN_FALSE;
            }
        }
    }
    else
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Expression array does not match required \"%s\" call signature: too "
                 "many arguments.",
                 context->log_name, module_name, owner_expression->source_name, (long) owner_expression->source_line,
                 get_expression_call_name_for_logging (owner_expression))
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline kan_bool_t resolve_expression_array_with_signature (
    struct rpl_compiler_context_t *context,
    struct rpl_compiler_instance_t *instance,
    struct kan_rpl_intermediate_t *intermediate,
    struct resolve_expression_scope_t *resolve_scope,
    struct compiler_instance_expression_node_t *target_expression,
    struct compiler_instance_expression_list_item_t **first_expression_output,
    kan_rpl_size_t expression_list_size,
    kan_rpl_size_t expression_list_index,
    struct compiler_instance_function_argument_node_t *first_argument)
{
    kan_bool_t resolved = KAN_TRUE;
    struct compiler_instance_expression_list_item_t *last_expression = NULL;
    struct compiler_instance_function_argument_node_t *current_argument = first_argument;
    kan_loop_size_t current_argument_index = 0u;

    for (kan_loop_size_t index = 0u; index < expression_list_size; ++index)
    {
        struct compiler_instance_expression_node_t *resolved_expression;
        const kan_rpl_size_t expression_index =
            ((kan_rpl_size_t *) intermediate->expression_lists_storage.data)[expression_list_index + index];

        if (resolve_expression (
                context, instance, intermediate, resolve_scope,
                &((struct kan_rpl_expression_t *) intermediate->expression_storage.data)[expression_index],
                &resolved_expression))
        {
            KAN_ASSERT (resolved_expression)
            struct compiler_instance_expression_list_item_t *list_item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                &instance->resolve_allocator, struct compiler_instance_expression_list_item_t);

            list_item->next = NULL;
            list_item->expression = resolved_expression;

            if (last_expression)
            {
                last_expression->next = list_item;
            }
            else
            {
                *first_expression_output = list_item;
            }

            last_expression = list_item;
            if (!resolve_match_signature_at_index (context, resolve_scope->function->module_name, target_expression,
                                                   current_argument ? &current_argument->variable.type : NULL,
                                                   current_argument_index, resolved_expression))
            {
                resolved = KAN_FALSE;
            }

            current_argument = current_argument ? current_argument->next : current_argument;
            ++current_argument_index;
        }
        else
        {
            resolved = KAN_FALSE;
        }
    }

    if (current_argument)
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Expression array does not match required \"%s\" call signature: not enough arguments.",
                 context->log_name, resolve_scope->function->module_name, target_expression->source_name,
                 (long) target_expression->source_line, get_expression_call_name_for_logging (target_expression))
        resolved = KAN_FALSE;
    }

    return resolved;
}

static struct compiler_instance_expression_node_t *resolve_find_loop_in_current_context (
    struct resolve_expression_scope_t *resolve_scope)
{
    if (resolve_scope->associated_outer_loop_if_any)
    {
        return resolve_scope->associated_outer_loop_if_any;
    }

    if (resolve_scope->parent)
    {
        return resolve_find_loop_in_current_context (resolve_scope->parent);
    }

    return NULL;
}

static kan_bool_t resolve_function_by_name (struct rpl_compiler_context_t *context,
                                            struct rpl_compiler_instance_t *instance,
                                            kan_interned_string_t function_name,
                                            enum kan_rpl_pipeline_stage_t context_stage,
                                            struct compiler_instance_function_node_t **output_node);

static struct resolve_fiend_access_linear_node_t *resolve_field_access_linearize_access_chain (
    struct rpl_compiler_context_t *context,
    struct kan_rpl_intermediate_t *intermediate,
    struct kan_rpl_expression_t *current_expression,
    struct kan_rpl_expression_t **stop_output)
{
    struct resolve_fiend_access_linear_node_t *first_node = NULL;
    while (KAN_TRUE)
    {
        struct kan_rpl_expression_t *input_child =
            &((struct kan_rpl_expression_t *)
                  intermediate->expression_storage.data)[current_expression->binary_operation.left_operand_index];

        struct kan_rpl_expression_t *field_child =
            &((struct kan_rpl_expression_t *)
                  intermediate->expression_storage.data)[current_expression->binary_operation.right_operand_index];

        if (field_child->type != KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot execute \".\" operation: right operand is not a field identifier.",
                     context->log_name, intermediate->log_name, current_expression->source_name,
                     (long) current_expression->source_line)
            return NULL;
        }

        struct resolve_fiend_access_linear_node_t *new_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &context->resolve_allocator, struct resolve_fiend_access_linear_node_t);

        new_node->next = first_node;
        first_node = new_node;
        new_node->field_source = field_child;

        if (input_child->type == KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION &&
            input_child->binary_operation.operation == KAN_RPL_BINARY_OPERATION_FIELD_ACCESS)
        {
            current_expression = input_child;
        }
        else
        {
            *stop_output = input_child;
            break;
        }
    }

    return first_node;
}

static enum kan_rpl_access_class_t get_container_access_for_stage (struct compiler_instance_container_node_t *container,
                                                                   enum kan_rpl_pipeline_stage_t stage)
{
    switch (container->type)
    {
    case KAN_RPL_CONTAINER_TYPE_VERTEX_ATTRIBUTE:
    case KAN_RPL_CONTAINER_TYPE_INSTANCED_ATTRIBUTE:
        // Always read only when accessible.
        return KAN_RPL_ACCESS_CLASS_READ_ONLY;

    case KAN_RPL_CONTAINER_TYPE_STATE:
        switch (stage)
        {
        case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX:
            return KAN_RPL_ACCESS_CLASS_WRITE_ONLY;

        case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT:
            return KAN_RPL_ACCESS_CLASS_READ_ONLY;
        }

        break;

    case KAN_RPL_CONTAINER_TYPE_COLOR_OUTPUT:
        // Always write only when accessible.
        return KAN_RPL_ACCESS_CLASS_WRITE_ONLY;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_RPL_ACCESS_CLASS_READ_ONLY;
}

static inline kan_bool_t resolve_container_field_access (
    struct rpl_compiler_context_t *context,
    struct rpl_compiler_instance_t *instance,
    struct resolve_expression_scope_t *resolve_scope,
    kan_rpl_size_t stop_expression_line,
    struct compiler_instance_container_node_t *container,
    struct resolve_fiend_access_linear_node_t *chain_first,
    struct compiler_instance_container_field_node_t **output_field,
    struct compiler_instance_type_definition_t *output_type,
    struct resolve_fiend_access_linear_node_t **output_access_resolve_next_node)
{
    KAN_ASSERT (chain_first)
    *output_field = NULL;
    *output_access_resolve_next_node = NULL;

    if (!resolve_use_container (context, instance, resolve_scope->function, resolve_scope->function->required_stage,
                                container, stop_expression_line))
    {
        return KAN_FALSE;
    }

    struct resolve_fiend_access_linear_node_t *chain_current = chain_first;
    const enum kan_rpl_access_class_t access =
        get_container_access_for_stage (container, resolve_scope->function->required_stage);
    enum kan_rpl_access_class_t resolved_access = access;

    switch (access)
    {
    case KAN_RPL_ACCESS_CLASS_READ_ONLY:
    case KAN_RPL_ACCESS_CLASS_WRITE_ONLY:
        // Access is one-directional, no need to access specifier.
        resolved_access = access;
        break;

    case KAN_RPL_ACCESS_CLASS_READ_WRITE:
        // This stage has bidirectional access to the container. Therefore, proper access must be specified.
        if (chain_current->field_source->identifier == STATICS.interned_in)
        {
            resolved_access = KAN_RPL_ACCESS_CLASS_READ_ONLY;
            chain_current = chain_current->next;
        }
        else if (chain_current->field_source->identifier == STATICS.interned_out)
        {
            resolved_access = KAN_RPL_ACCESS_CLASS_WRITE_ONLY;
            chain_current = chain_current->next;
        }
        else
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Failed to resolve container \"%s\" access: expected \"in\" or \"out\" abstract "
                     "field to specify access direction as container is accessible both ways in stage \"%s\".",
                     context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
                     (long) chain_current->field_source->source_line, container->name,
                     get_stage_name (resolve_scope->function->required_stage))
            return KAN_FALSE;
        }

        if (!chain_current)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Failed to resolve container \"%s\" access: expected field name after access "
                     "direction abstract field.",
                     context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
                     (long) chain_current->field_source->source_line, container->name)
            return KAN_FALSE;
        }

        break;
    }

    struct compiler_instance_container_field_node_t *field = container->first_field;
    while (field)
    {
        if (field->variable.name == chain_current->field_source->identifier)
        {
            struct compiler_instance_container_field_stage_node_t *stage_node = field->first_usage_stage;
            while (stage_node)
            {
                if (stage_node->user_stage == resolve_scope->function->required_stage)
                {
                    break;
                }

                stage_node = stage_node->next;
            }

            if (!stage_node)
            {
                stage_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                    &instance->resolve_allocator, struct compiler_instance_container_field_stage_node_t);

                stage_node->user_stage = resolve_scope->function->required_stage;
                stage_node->spirv_id_input = SPIRV_FIXED_ID_INVALID;
                stage_node->spirv_id_output = SPIRV_FIXED_ID_INVALID;

                stage_node->next = field->first_usage_stage;
                field->first_usage_stage = stage_node;
            }

            copy_type_definition (output_type, &field->variable.type);
            output_type->access = resolved_access;
            output_type->flags |= COMPILER_INSTANCE_TYPE_INTERFACE_POINTER;

            *output_field = field;
            *output_access_resolve_next_node = chain_current->next;
            return KAN_TRUE;
        }

        field = field->next;
    }

    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
             "[%s:%s:%s:%ld] Failed to resolve container \"%s\" access: no field \"%s\".", context->log_name,
             resolve_scope->function->module_name, chain_first->field_source->source_name,
             (long) chain_current->field_source->source_line, container->name, chain_first->field_source->identifier)
    return KAN_FALSE;
}

static inline kan_bool_t resolve_field_access_structured (struct rpl_compiler_context_t *context,
                                                          struct rpl_compiler_instance_t *instance,
                                                          struct resolve_expression_scope_t *resolve_scope,
                                                          struct compiler_instance_expression_node_t *input_node,
                                                          struct resolve_fiend_access_linear_node_t *chain_first,
                                                          kan_instance_size_t chain_length,
                                                          struct compiler_instance_expression_node_t *result_expression)
{
    if (input_node->output.array_size_runtime || input_node->output.array_dimensions_count > 0u)
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Failed to resolve structured access: attempted to use \".\" on array.",
                 context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
                 (long) chain_first->field_source->source_line)
        return KAN_FALSE;
    }

    result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS;
    result_expression->structured_access.input = input_node;
    result_expression->structured_access.access_chain_length = chain_length;
    result_expression->structured_access.access_chain_indices = kan_stack_group_allocator_allocate (
        &instance->resolve_allocator, sizeof (kan_instance_size_t) * chain_length, _Alignof (kan_instance_size_t));

    copy_type_definition (&result_expression->output, &input_node->output);
    kan_instance_size_t chain_output_index = 0u;
    kan_bool_t increment_chain_output_index = KAN_TRUE;
    struct resolve_fiend_access_linear_node_t *chain_current = chain_first;

    while (chain_current)
    {
        kan_bool_t found = KAN_FALSE;
        if (result_expression->output.array_size_runtime || result_expression->output.array_dimensions_count > 0u)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Failed to resolve structured access: attempted to use \".\" on array.",
                     context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
                     (long) chain_first->field_source->source_line)
            return KAN_FALSE;
        }

        switch (result_expression->output.class)
        {
        case COMPILER_INSTANCE_TYPE_CLASS_VOID:
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Failed to resolve structured access: attempted to use \".\" on void.",
                     context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
                     (long) chain_first->field_source->source_line)
            return KAN_FALSE;

        case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
        {
            if (chain_current->field_source->identifier[0u] == '\0')
            {
                KAN_LOG (
                    rpl_compiler_context, KAN_LOG_ERROR,
                    "[%s:%s:%s:%ld] Failed to resolve structured access: vector component access specifier is empty.",
                    context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
                    (long) chain_first->field_source->source_line)
                return KAN_FALSE;
            }

            if (chain_current->field_source->identifier[1u] == '\0')
            {
                switch (chain_current->field_source->identifier[0u])
                {
                case 'x':
                    result_expression->structured_access.access_chain_indices[chain_output_index] = 0u;
                    break;
                case 'y':
                    result_expression->structured_access.access_chain_indices[chain_output_index] = 1u;
                    break;
                case 'z':
                    result_expression->structured_access.access_chain_indices[chain_output_index] = 2u;
                    break;
                case 'w':
                    result_expression->structured_access.access_chain_indices[chain_output_index] = 3u;
                    break;
                default:
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Failed to resolve structured access: unknown vector component access "
                             "specified \"%s\", only \"x\", \"y\", \"z\" and \"w\" are supported for 0th, 1st, 2nd and "
                             "3rd components respectively.",
                             context->log_name, resolve_scope->function->module_name,
                             chain_first->field_source->source_name, (long) chain_first->field_source->source_line,
                             chain_current->field_source->identifier)
                    return KAN_FALSE;
                }

                if (result_expression->structured_access.access_chain_indices[chain_output_index] >=
                    result_expression->output.vector_data->items_count)
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Failed to resolve structured access: \"%s\" has only %ld components, but "
                             "component at index %ld requested.",
                             context->log_name, resolve_scope->function->module_name,
                             chain_first->field_source->source_name, (long) chain_first->field_source->source_line,
                             result_expression->output.matrix_data->name,
                             (long) result_expression->output.matrix_data->columns,
                             (long) result_expression->structured_access.access_chain_indices[chain_output_index])
                    return KAN_FALSE;
                }

                found = KAN_TRUE;
                result_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_VECTOR;
                result_expression->output.vector_data =
                    &STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (result_expression->output.matrix_data->item, 1u)];
            }
            else
            {
                if (!is_access_readable (result_expression->output.access))
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Failed to resolve structured access: vector swizzle result is always read "
                             "only, therefore its source must be readable, but it isn't.",
                             context->log_name, resolve_scope->function->module_name,
                             chain_first->field_source->source_name, (long) chain_first->field_source->source_line)
                    return KAN_FALSE;
                }

                struct compiler_instance_expression_node_t *swizzle_input_node = input_node;
                if (chain_current != chain_first)
                {
                    // Copy current structured access into separate node and use it as input for swizzle instead.
                    struct compiler_instance_expression_node_t *new_expression =
                        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&instance->resolve_allocator,
                                                                  struct compiler_instance_expression_node_t);

                    new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS;
                    new_expression->structured_access = result_expression->structured_access;
                    new_expression->structured_access.access_chain_length = chain_output_index;
                    copy_type_definition (&new_expression->output, &result_expression->output);

                    new_expression->module_name = result_expression->module_name;
                    new_expression->source_name = result_expression->source_name;
                    new_expression->source_line = result_expression->source_line;
                    swizzle_input_node = new_expression;
                }

                struct compiler_instance_expression_node_t *swizzle_node = NULL;
                if (chain_current->next)
                {
                    // Swizzle is not the last expression. Therefore, we need a new expression for it.
                    struct compiler_instance_expression_node_t *new_expression =
                        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&instance->resolve_allocator,
                                                                  struct compiler_instance_expression_node_t);

                    swizzle_node->output.array_size_runtime = KAN_FALSE;
                    swizzle_node->output.array_dimensions_count = 0u;
                    swizzle_node->output.array_dimensions = NULL;
                    swizzle_node->output.flags = 0u;

                    new_expression->module_name = result_expression->module_name;
                    new_expression->source_name = result_expression->source_name;
                    new_expression->source_line = result_expression->source_line;
                    swizzle_node = new_expression;
                }
                else
                {
                    // Swizzle is the last expression, we can write it directly to the result.
                    swizzle_node = result_expression;
                }

                swizzle_node->type = COMPILER_INSTANCE_EXPRESSION_TYPE_SWIZZLE;
                swizzle_node->swizzle.input = swizzle_input_node;
                swizzle_node->swizzle.items_count = 0u;

                while (chain_current->field_source->identifier[swizzle_node->swizzle.items_count] != '\0')
                {
                    if (swizzle_node->swizzle.items_count >= SWIZZLE_MAX_ITEMS)
                    {
                        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                                 "[%s:%s:%s:%ld] Failed to resolve structured access: too many swizzle items, only up "
                                 "to %u supported.",
                                 context->log_name, resolve_scope->function->module_name,
                                 chain_first->field_source->source_name, (long) chain_first->field_source->source_line,
                                 (unsigned) SWIZZLE_MAX_ITEMS)
                        return KAN_FALSE;
                    }

                    switch (chain_current->field_source->identifier[swizzle_node->swizzle.items_count])
                    {
                    case 'x':
                        swizzle_node->swizzle.items[swizzle_node->swizzle.items_count] = 0u;
                        break;
                    case 'y':
                        swizzle_node->swizzle.items[swizzle_node->swizzle.items_count] = 1u;
                        break;
                    case 'z':
                        swizzle_node->swizzle.items[swizzle_node->swizzle.items_count] = 2u;
                        break;
                    case 'w':
                        swizzle_node->swizzle.items[swizzle_node->swizzle.items_count] = 3u;
                        break;
                    default:
                        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                                 "[%s:%s:%s:%ld] Failed to resolve structured access: unknown vector component access "
                                 "in swizzle specified \"%c\", only \"x\", \"y\", \"z\" and \"w\" are supported for "
                                 "0th, 1st, 2nd and 3rd components respectively.",
                                 context->log_name, resolve_scope->function->module_name,
                                 chain_first->field_source->source_name, (long) chain_first->field_source->source_line,
                                 chain_current->field_source->identifier[swizzle_node->swizzle.items_count])
                        return KAN_FALSE;
                    }

                    ++swizzle_node->swizzle.items_count;
                }

                // Otherwise we're doing something wrong internally.
                KAN_ASSERT (swizzle_node->swizzle.items_count > 1u)

                found = KAN_TRUE;
                // Swizzles cannot be targets for writing.
                swizzle_node->output.access = KAN_RPL_ACCESS_CLASS_READ_ONLY;
                // Swizzles no longer reference any interface data as it is a new object.
                swizzle_node->output.flags &= ~COMPILER_INSTANCE_TYPE_INTERFACE_POINTER;

                result_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_VECTOR;
                result_expression->output.vector_data = &STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (
                    result_expression->output.matrix_data->item, swizzle_node->swizzle.items_count)];

                if (swizzle_node != result_expression)
                {
                    // Swizzle node is not a result expression. We need to properly rebuild result structured access.
                    result_expression->structured_access.input = swizzle_node;
                    KAN_ASSERT (result_expression->structured_access.access_chain_length > chain_output_index + 1u)
                    result_expression->structured_access.access_chain_length -= chain_output_index - 1u;
                    result_expression->structured_access.access_chain_indices += chain_output_index + 1u;

                    copy_type_definition (&result_expression->output, &swizzle_node->output);
                    result_expression->output.access = KAN_RPL_ACCESS_CLASS_READ_ONLY;
                    result_expression->output.flags &= ~COMPILER_INSTANCE_TYPE_INTERFACE_POINTER;

                    chain_output_index = 0u;
                    increment_chain_output_index = KAN_FALSE;
                }
            }

            break;
        }

        case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
        {
            if (chain_current->field_source->identifier[0u] == '\0')
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve structured access: matrix column access specifier is empty.",
                         context->log_name, resolve_scope->function->module_name,
                         chain_first->field_source->source_name, (long) chain_first->field_source->source_line)
                return KAN_FALSE;
            }

            if (chain_current->field_source->identifier[1u] != '\0')
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve structured access: matrix column access specifier is longer "
                         "than 1 symbol.",
                         context->log_name, resolve_scope->function->module_name,
                         chain_first->field_source->source_name, (long) chain_first->field_source->source_line)
                return KAN_FALSE;
            }

            switch (chain_current->field_source->identifier[0u])
            {
            case 'x':
                result_expression->structured_access.access_chain_indices[chain_output_index] = 0u;
                break;
            case 'y':
                result_expression->structured_access.access_chain_indices[chain_output_index] = 1u;
                break;
            case 'z':
                result_expression->structured_access.access_chain_indices[chain_output_index] = 2u;
                break;
            case 'w':
                result_expression->structured_access.access_chain_indices[chain_output_index] = 3u;
                break;
            default:
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve structured access: unknown matrix column access specified "
                         "\"%s\", only \"x\", \"y\", \"z\" and \"w\" are supported for 0th, 1st, 2nd and 3rd columns "
                         "respectively.",
                         context->log_name, resolve_scope->function->module_name,
                         chain_first->field_source->source_name, (long) chain_first->field_source->source_line,
                         chain_current->field_source->identifier)
                return KAN_FALSE;
            }

            if (result_expression->structured_access.access_chain_indices[chain_output_index] >=
                result_expression->output.matrix_data->columns)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve structured access: \"%s\" has only %ld columns, but column "
                         "at index %ld requested.",
                         context->log_name, resolve_scope->function->module_name,
                         chain_first->field_source->source_name, (long) chain_first->field_source->source_line,
                         result_expression->output.matrix_data->name,
                         (long) result_expression->output.matrix_data->columns,
                         (long) result_expression->structured_access.access_chain_indices[chain_output_index])
                return KAN_FALSE;
            }

            found = KAN_TRUE;
            result_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_VECTOR;
            result_expression->output.vector_data = &STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (
                result_expression->output.matrix_data->item, result_expression->output.matrix_data->rows)];

            break;
        }

#define SEARCH_USING_DECLARATION                                                                                       \
    result_expression->structured_access.access_chain_indices[chain_output_index] = 0u;                                \
    while (declaration)                                                                                                \
    {                                                                                                                  \
        if (declaration->variable.name == chain_current->field_source->identifier)                                     \
        {                                                                                                              \
            found = KAN_TRUE;                                                                                          \
            enum compiler_instance_type_flags_t old_flags = result_expression->output.flags;                           \
            copy_type_definition (&result_expression->output, &declaration->variable.type);                            \
            result_expression->output.flags |= old_flags;                                                              \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        ++result_expression->structured_access.access_chain_indices[chain_output_index];                               \
        declaration = declaration->next;                                                                               \
    }

        case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
        {
            struct compiler_instance_declaration_node_t *declaration =
                result_expression->output.struct_data->first_field;
            SEARCH_USING_DECLARATION
            break;
        }

        case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
        {
            struct compiler_instance_declaration_node_t *declaration = input_node->output.buffer_data->first_field;
            SEARCH_USING_DECLARATION
            break;
        }

#undef SEARCH_USING_DECLARATION

        case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Failed to resolve structured access: attempted to use \".\" on boolean.",
                     context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
                     (long) chain_first->field_source->source_line)
            return KAN_FALSE;

        case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Failed to resolve structured access: attempted to use \".\" on sampler.",
                     context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
                     (long) chain_first->field_source->source_line)
            return KAN_FALSE;

        case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Failed to resolve structured access: attempted to use \".\" on image.",
                     context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
                     (long) chain_first->field_source->source_line)
            return KAN_FALSE;
        }

        if (!found)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Failed to resolve structured access at field \"%s\": no path for such field.",
                     context->log_name, resolve_scope->function->module_name, chain_current->field_source->source_name,
                     (long) chain_current->field_source->source_line, chain_current->field_source->identifier)
            return KAN_FALSE;
        }

        if (increment_chain_output_index)
        {
            ++chain_output_index;
        }

        increment_chain_output_index = KAN_TRUE;
        chain_current = chain_current->next;
    }

    return KAN_TRUE;
}

static inline kan_bool_t resolve_binary_operation (struct rpl_compiler_context_t *context,
                                                   struct rpl_compiler_instance_t *instance,
                                                   struct kan_rpl_intermediate_t *intermediate,
                                                   struct resolve_expression_scope_t *resolve_scope,
                                                   struct kan_rpl_expression_t *input_expression,
                                                   struct compiler_instance_expression_node_t *result_expression)
{
    // Field access parse into appropriate access operation is complicated and therefore separated from everything else.
    if (input_expression->binary_operation.operation == KAN_RPL_BINARY_OPERATION_FIELD_ACCESS)
    {
        struct kan_rpl_expression_t *chain_stop_expression;
        struct resolve_fiend_access_linear_node_t *chain_first = resolve_field_access_linearize_access_chain (
            context, intermediate, input_expression, &chain_stop_expression);

        if (!chain_first)
        {
            return KAN_FALSE;
        }

        struct compiler_instance_expression_node_t *chain_input_expression = NULL;

        // If chain stop points to a container, we need to resolve container access.
        if (chain_stop_expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER)
        {
            struct compiler_instance_container_node_t *container = instance->first_container;
            while (container)
            {
                if (container->name == chain_stop_expression->identifier)
                {
                    struct compiler_instance_container_field_node_t *field_node;
                    struct compiler_instance_type_definition_t access_output_type;

                    if (!resolve_container_field_access (context, instance, resolve_scope,
                                                         chain_stop_expression->source_line, container, chain_first,
                                                         &field_node, &access_output_type, &chain_first))
                    {
                        return KAN_FALSE;
                    }

                    if (chain_first)
                    {
                        chain_input_expression = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                            &instance->resolve_allocator, struct compiler_instance_expression_node_t);

                        chain_input_expression->type =
                            access_output_type.access == KAN_RPL_ACCESS_CLASS_READ_ONLY ?
                                COMPILER_INSTANCE_EXPRESSION_TYPE_CONTAINER_FIELD_ACCESS_INPUT :
                                COMPILER_INSTANCE_EXPRESSION_TYPE_CONTAINER_FIELD_ACCESS_OUTPUT;

                        chain_input_expression->container_field_access = field_node;
                        chain_input_expression->module_name = resolve_scope->function->module_name;
                        chain_input_expression->source_name = chain_stop_expression->source_name;
                        chain_input_expression->source_line = chain_stop_expression->source_line;

                        copy_type_definition (&chain_input_expression->output, &access_output_type);
                    }
                    else
                    {
                        // Full access chain was resolved as flattened access.
                        result_expression->type = access_output_type.access == KAN_RPL_ACCESS_CLASS_READ_ONLY ?
                                                      COMPILER_INSTANCE_EXPRESSION_TYPE_CONTAINER_FIELD_ACCESS_INPUT :
                                                      COMPILER_INSTANCE_EXPRESSION_TYPE_CONTAINER_FIELD_ACCESS_OUTPUT;

                        result_expression->container_field_access = field_node;
                        copy_type_definition (&result_expression->output, &access_output_type);
                        return KAN_TRUE;
                    }

                    break;
                }

                container = container->next;
            }
        }

        if (!chain_input_expression && !resolve_expression (context, instance, intermediate, resolve_scope,
                                                            chain_stop_expression, &chain_input_expression))
        {
            return KAN_FALSE;
        }

        kan_loop_size_t chain_length = 0u;
        struct resolve_fiend_access_linear_node_t *chain_item = chain_first;

        while (chain_item)
        {
            ++chain_length;
            chain_item = chain_item->next;
        }

        return resolve_field_access_structured (context, instance, resolve_scope, chain_input_expression, chain_first,
                                                (kan_instance_size_t) chain_length, result_expression);
    }

    if (!resolve_expression (context, instance, intermediate, resolve_scope,
                             &((struct kan_rpl_expression_t *) intermediate->expression_storage
                                   .data)[input_expression->binary_operation.left_operand_index],
                             &result_expression->binary_operation.left_operand))
    {
        return KAN_FALSE;
    }

    if (!resolve_expression (context, instance, intermediate, resolve_scope,
                             &((struct kan_rpl_expression_t *) intermediate->expression_storage
                                   .data)[input_expression->binary_operation.right_operand_index],
                             &result_expression->binary_operation.right_operand))
    {
        return KAN_FALSE;
    }

    struct compiler_instance_expression_node_t *left = result_expression->binary_operation.left_operand;
    struct compiler_instance_expression_node_t *right = result_expression->binary_operation.right_operand;

    switch (input_expression->binary_operation.operation)
    {
    case KAN_RPL_BINARY_OPERATION_FIELD_ACCESS:
        // Should be processed separately in upper segment.
        KAN_ASSERT (KAN_FALSE)
        return KAN_FALSE;

#define NEEDS_TO_READ_LEFT(OPERATOR_STRING)                                                                            \
    if (!is_access_readable (left->output.access))                                                                     \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATOR_STRING                                                    \
                 "\" operation as left argument must be readable, but it isn't.",                                      \
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,               \
                 (long) input_expression->source_line)                                                                 \
        return KAN_FALSE;                                                                                              \
    }

#define NEEDS_TO_READ_RIGHT(OPERATOR_STRING)                                                                           \
    if (!is_access_readable (right->output.access))                                                                    \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATOR_STRING                                                    \
                 "\" operation as right argument must be readable, but it isn't.",                                     \
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,               \
                 (long) input_expression->source_line)                                                                 \
        return KAN_FALSE;                                                                                              \
    }

    case KAN_RPL_BINARY_OPERATION_ARRAY_ACCESS:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ARRAY_INDEX;
        NEEDS_TO_READ_RIGHT ("[]")

        if (!left->output.array_size_runtime && left->output.array_dimensions_count == 0u)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot execute array access as left operand in not an array.", context->log_name,
                     resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line)
            return KAN_FALSE;
        }

        if (right->output.class != COMPILER_INSTANCE_TYPE_CLASS_VECTOR ||
            right->output.vector_data->item != INBUILT_TYPE_ITEM_UNSIGNED ||
            right->output.vector_data->items_count > 1u)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot execute array access as right operand is \"%s\" instead of u1.",
                     context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line, get_type_name_for_logging (&right->output))
            return KAN_FALSE;
        }

        copy_type_definition (&result_expression->output, &left->output);
        result_expression->output.array_size_runtime = KAN_FALSE;
        result_expression->output.array_dimensions_count =
            left->output.array_size_runtime ? 0u : left->output.array_dimensions_count - 1u;
        result_expression->output.array_dimensions = left->output.array_dimensions + 1u;
        return KAN_TRUE;

#define CANNOT_EXECUTE_ON_ARRAYS(OPERATOR_STRING)                                                                      \
    if (left->output.array_size_runtime || left->output.array_dimensions_count != 0u ||                                \
        right->output.array_size_runtime || right->output.array_dimensions_count != 0u)                                \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATOR_STRING "\" operation on arrays.", context->log_name,      \
                 resolve_scope->function->module_name, input_expression->source_name,                                  \
                 (long) input_expression->source_line)                                                                 \
        return KAN_FALSE;                                                                                              \
    }

#define CAN_ONLY_EXECUTE_ON_MATCHING_BUILTIN(OPERATOR_STRING)                                                          \
    {                                                                                                                  \
        const kan_bool_t is_matching_builtin = left->output.class == right->output.class &&                            \
                                               ((left->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&          \
                                                 left->output.vector_data == right->output.vector_data) ||             \
                                                (left->output.class == COMPILER_INSTANCE_TYPE_CLASS_MATRIX &&          \
                                                 left->output.matrix_data == right->output.matrix_data));              \
                                                                                                                       \
        if (!is_matching_builtin)                                                                                      \
        {                                                                                                              \
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                              \
                     "[%s:%s:%s:%ld] Cannot execute \"" OPERATOR_STRING "\" on \"%s\" and \"%s\".", context->log_name, \
                     resolve_scope->function->module_name, input_expression->source_name,                              \
                     (long) input_expression->source_line, get_type_name_for_logging (&left->output),                  \
                     get_type_name_for_logging (&right->output))                                                       \
            return KAN_FALSE;                                                                                          \
        }                                                                                                              \
    }

#define COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION                                                                    \
    copy_type_definition (&result_expression->output, &left->output);                                                  \
    result_expression->output.access = KAN_RPL_ACCESS_CLASS_READ_ONLY;                                                 \
    result_expression->output.flags &= ~COMPILER_INSTANCE_TYPE_INTERFACE_POINTER

#define COPY_TYPE_FROM_RIGHT_FOR_ELEMENTAL_OPERATION                                                                   \
    copy_type_definition (&result_expression->output, &right->output);                                                 \
    result_expression->output.access = KAN_RPL_ACCESS_CLASS_READ_ONLY;                                                 \
    result_expression->output.flags &= ~COMPILER_INSTANCE_TYPE_INTERFACE_POINTER

    case KAN_RPL_BINARY_OPERATION_ADD:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ADD;
        CANNOT_EXECUTE_ON_ARRAYS ("+")
        CAN_ONLY_EXECUTE_ON_MATCHING_BUILTIN ("+")
        NEEDS_TO_READ_LEFT ("+")
        NEEDS_TO_READ_RIGHT ("+")
        COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_SUBTRACT:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_SUBTRACT;
        CANNOT_EXECUTE_ON_ARRAYS ("-")
        CAN_ONLY_EXECUTE_ON_MATCHING_BUILTIN ("-")
        NEEDS_TO_READ_LEFT ("-")
        NEEDS_TO_READ_RIGHT ("-")
        COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_MULTIPLY:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_MULTIPLY;
        CANNOT_EXECUTE_ON_ARRAYS ("*")
        NEEDS_TO_READ_LEFT ("*")
        NEEDS_TO_READ_RIGHT ("*")

        // Multiply vectors by elements.
        if (left->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
            right->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
            left->output.vector_data == right->output.vector_data)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Multiply vector by scalar of the same type.
        if (left->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
            right->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
            left->output.vector_data->item == right->output.vector_data->item &&
            right->output.vector_data->items_count == 1u)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Multiply matrix by scalar of the same type.
        if (left->output.class == COMPILER_INSTANCE_TYPE_CLASS_MATRIX &&
            right->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
            left->output.matrix_data->item == right->output.vector_data->item &&
            right->output.vector_data->items_count == 1u)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Multiply matrix by vector of the same type.
        if (left->output.class == COMPILER_INSTANCE_TYPE_CLASS_MATRIX &&
            right->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
            left->output.matrix_data->item == right->output.vector_data->item &&
            left->output.matrix_data->columns == right->output.vector_data->items_count)
        {
            COPY_TYPE_FROM_RIGHT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Multiply vector by matrix of the same type.
        if (left->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
            right->output.class == COMPILER_INSTANCE_TYPE_CLASS_MATRIX &&
            left->output.vector_data->item == right->output.matrix_data->item &&
            left->output.vector_data->items_count == right->output.matrix_data->rows)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Multiply matrix by matrix of the same type.
        if (left->output.class == COMPILER_INSTANCE_TYPE_CLASS_MATRIX &&
            right->output.class == COMPILER_INSTANCE_TYPE_CLASS_MATRIX &&
            left->output.matrix_data->item == right->output.matrix_data->item &&
            left->output.matrix_data->columns == right->output.matrix_data->rows)
        {
            result_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_MATRIX;
            result_expression->output.matrix_data = NULL;

            const kan_instance_size_t matrix_type_count =
                sizeof (kan_rpl_compiler_statics.matrix_types) / sizeof (kan_rpl_compiler_statics.matrix_types[0u]);

            for (kan_loop_size_t index = 0u; index < matrix_type_count; ++index)
            {
                struct inbuilt_matrix_type_t *type = &kan_rpl_compiler_statics.matrix_types[index];
                if (type->item == left->output.matrix_data->item && type->rows == left->output.matrix_data->rows &&
                    type->columns == right->output.matrix_data->columns)
                {
                    result_expression->output.matrix_data = type;
                }
            }

            if (!result_expression->output.matrix_data)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] There is no supported matrix type to represent result of \"*\" on \"%s\" and "
                         "\"%s\".",
                         context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                         (long) input_expression->source_line, get_type_name_for_logging (&left->output),
                         get_type_name_for_logging (&right->output))
                return KAN_FALSE;
            }

            result_expression->output.access = KAN_RPL_ACCESS_CLASS_READ_ONLY;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Cannot execute \"*\" on \"%s\" and \"%s\".",
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                 (long) input_expression->source_line, get_type_name_for_logging (&left->output),
                 get_type_name_for_logging (&right->output))
        return KAN_FALSE;

    case KAN_RPL_BINARY_OPERATION_DIVIDE:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_DIVIDE;
        CANNOT_EXECUTE_ON_ARRAYS ("/")
        NEEDS_TO_READ_LEFT ("/")
        NEEDS_TO_READ_RIGHT ("/")

        // Divide vectors of the same type.
        if (left->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
            right->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
            left->output.vector_data == right->output.vector_data)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Divide floating point matrices (integer point matrices are left out for simplicity).
        if (left->output.class == COMPILER_INSTANCE_TYPE_CLASS_MATRIX &&
            right->output.class == COMPILER_INSTANCE_TYPE_CLASS_MATRIX &&
            left->output.matrix_data == right->output.matrix_data)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Divide vector by scalar of the same type.
        if (left->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
            right->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
            left->output.vector_data->item == right->output.vector_data->item &&
            right->output.vector_data->items_count == 1u)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Cannot execute \"/\" on \"%s\" and \"%s\".",
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                 (long) input_expression->source_line, get_type_name_for_logging (&left->output),
                 get_type_name_for_logging (&right->output))
        return KAN_FALSE;

#define INTEGER_ONLY_VECTOR_OPERATION(OPERATION_STRING)                                                                \
    CANNOT_EXECUTE_ON_ARRAYS (OPERATION_STRING)                                                                        \
    CAN_ONLY_EXECUTE_ON_MATCHING_BUILTIN (OPERATION_STRING)                                                            \
    NEEDS_TO_READ_LEFT (OPERATION_STRING)                                                                              \
    NEEDS_TO_READ_RIGHT (OPERATION_STRING)                                                                             \
                                                                                                                       \
    if (left->output.class != COMPILER_INSTANCE_TYPE_CLASS_VECTOR ||                                                   \
        right->output.class != COMPILER_INSTANCE_TYPE_CLASS_VECTOR ||                                                  \
        !inbuilt_type_item_is_integer (left->output.vector_data->item) ||                                              \
        !inbuilt_type_item_is_integer (right->output.vector_data->item))                                               \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATION_STRING                                                   \
                 "\" on \"%s\" and \"%s\", only unsigned and signed vectors are supported.",                           \
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,               \
                 (long) input_expression->source_line, get_type_name_for_logging (&left->output),                      \
                 get_type_name_for_logging (&right->output))                                                           \
    }                                                                                                                  \
                                                                                                                       \
    COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION

    case KAN_RPL_BINARY_OPERATION_MODULUS:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_MODULUS;
        INTEGER_ONLY_VECTOR_OPERATION ("%%");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_ASSIGN:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ASSIGN;
        CANNOT_EXECUTE_ON_ARRAYS ("=")
        NEEDS_TO_READ_RIGHT ("=")

        if (!is_access_writeable (left->output.access))
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot execute \"=\" as its output is not writable.", context->log_name,
                     resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line)
            return KAN_FALSE;
        }

        if (!is_type_definition_base_equal (&left->output, &right->output))
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Cannot execute \"=\" on \"%s\" and \"%s\".",
                     context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line, get_type_name_for_logging (&left->output),
                     get_type_name_for_logging (&right->output))
            return KAN_FALSE;
        }

        COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
        return KAN_TRUE;

#define LOGIC_OPERATION(OPERATION_STRING)                                                                              \
    CANNOT_EXECUTE_ON_ARRAYS (OPERATION_STRING)                                                                        \
    NEEDS_TO_READ_LEFT (OPERATION_STRING)                                                                              \
    NEEDS_TO_READ_RIGHT (OPERATION_STRING)                                                                             \
                                                                                                                       \
    if (left->output.class != COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN ||                                                  \
        right->output.class != COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN)                                                   \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATION_STRING                                                   \
                 "\" on \"%s\" and \"%s\", only booleans are supported.",                                              \
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,               \
                 (long) input_expression->source_line, get_type_name_for_logging (&left->output),                      \
                 get_type_name_for_logging (&right->output))                                                           \
        return KAN_FALSE;                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    result_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN;

    case KAN_RPL_BINARY_OPERATION_AND:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_AND;
        LOGIC_OPERATION ("&&");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_OR:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_OR;
        LOGIC_OPERATION ("||");
        return KAN_TRUE;

#define EQUALITY_OPERATION(OPERATION_STRING)                                                                           \
    CANNOT_EXECUTE_ON_ARRAYS (OPERATION_STRING)                                                                        \
    CAN_ONLY_EXECUTE_ON_MATCHING_BUILTIN (OPERATION_STRING)                                                            \
    NEEDS_TO_READ_LEFT (OPERATION_STRING)                                                                              \
    NEEDS_TO_READ_RIGHT (OPERATION_STRING)                                                                             \
                                                                                                                       \
    if (left->output.class != COMPILER_INSTANCE_TYPE_CLASS_VECTOR ||                                                   \
        right->output.class != COMPILER_INSTANCE_TYPE_CLASS_VECTOR ||                                                  \
        !inbuilt_type_item_is_integer (left->output.vector_data->item) ||                                              \
        !inbuilt_type_item_is_integer (right->output.vector_data->item))                                               \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATION_STRING                                                   \
                 "\" on \"%s\" and \"%s\", only unsigned and signed vectors are supported.",                           \
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,               \
                 (long) input_expression->source_line, get_type_name_for_logging (&left->output),                      \
                 get_type_name_for_logging (&right->output))                                                           \
    }                                                                                                                  \
                                                                                                                       \
    result_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN;                                            \
    result_expression->output.array_size_runtime = KAN_FALSE;                                                          \
    result_expression->output.array_dimensions_count = 0u;                                                             \
    result_expression->output.array_dimensions = NULL;                                                                 \
    result_expression->output.access = KAN_RPL_ACCESS_CLASS_READ_ONLY;

    case KAN_RPL_BINARY_OPERATION_EQUAL:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_EQUAL;
        EQUALITY_OPERATION ("==");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_NOT_EQUAL:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NOT_EQUAL;
        EQUALITY_OPERATION ("!=");
        return KAN_TRUE;

#define LOGICAL_SCALAR_ONLY_OPERATION(OPERATION_STRING)                                                                \
    CANNOT_EXECUTE_ON_ARRAYS (OPERATION_STRING)                                                                        \
    NEEDS_TO_READ_LEFT (OPERATION_STRING)                                                                              \
    NEEDS_TO_READ_RIGHT (OPERATION_STRING)                                                                             \
                                                                                                                       \
    if (left->output.class != COMPILER_INSTANCE_TYPE_CLASS_VECTOR ||                                                   \
        right->output.class != COMPILER_INSTANCE_TYPE_CLASS_VECTOR || left->output.vector_data->items_count > 1u ||    \
        right->output.vector_data->items_count > 1u ||                                                                 \
        left->output.vector_data->item != right->output.vector_data->item)                                             \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATION_STRING                                                   \
                 "\" on \"%s\" and \"%s\", only one-item vectors are supported.",                                      \
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,               \
                 (long) input_expression->source_line, get_type_name_for_logging (&left->output),                      \
                 get_type_name_for_logging (&right->output))                                                           \
    }                                                                                                                  \
                                                                                                                       \
    result_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN;

    case KAN_RPL_BINARY_OPERATION_LESS:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_LESS;
        LOGICAL_SCALAR_ONLY_OPERATION ("<");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_GREATER:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_GREATER;
        LOGICAL_SCALAR_ONLY_OPERATION (">");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_LESS_OR_EQUAL:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_LESS_OR_EQUAL;
        LOGICAL_SCALAR_ONLY_OPERATION ("<=");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_GREATER_OR_EQUAL:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_GREATER_OR_EQUAL;
        LOGICAL_SCALAR_ONLY_OPERATION (">=");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_BITWISE_AND:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_AND;
        INTEGER_ONLY_VECTOR_OPERATION ("&");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_BITWISE_OR:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_OR;
        INTEGER_ONLY_VECTOR_OPERATION ("|");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_BITWISE_XOR:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_XOR;
        INTEGER_ONLY_VECTOR_OPERATION ("|");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_BITWISE_LSHIFT:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_LEFT_SHIFT;
        INTEGER_ONLY_VECTOR_OPERATION ("|");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_BITWISE_RSHIFT:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_RIGHT_SHIFT;
        INTEGER_ONLY_VECTOR_OPERATION ("|");
        return KAN_TRUE;

#undef NEEDS_TO_READ_LEFT
#undef NEEDS_TO_READ_RIGHT
#undef CANNOT_EXECUTE_ON_ARRAYS
#undef CAN_ONLY_EXECUTE_ON_MATCHING_BUILTIN
#undef COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION
#undef COPY_TYPE_FROM_RIGHT_FOR_ELEMENTAL_OPERATION
#undef INTEGER_ONLY_VECTOR_OPERATION
#undef EQUALITY_OPERATION
#undef LOGIC_OPERATION
#undef LOGICAL_SCALAR_ONLY_OPERATION
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t resolve_unary_operation (struct rpl_compiler_context_t *context,
                                                  struct rpl_compiler_instance_t *instance,
                                                  struct kan_rpl_intermediate_t *intermediate,
                                                  struct resolve_expression_scope_t *resolve_scope,
                                                  struct kan_rpl_expression_t *input_expression,
                                                  struct compiler_instance_expression_node_t *result_expression)
{
    if (!resolve_expression (context, instance, intermediate, resolve_scope,
                             &((struct kan_rpl_expression_t *) intermediate->expression_storage
                                   .data)[input_expression->unary_operation.operand_index],
                             &result_expression->unary_operation.operand))
    {
        return KAN_FALSE;
    }

    struct compiler_instance_expression_node_t *operand = result_expression->unary_operation.operand;
    if (operand->output.array_size_runtime || operand->output.array_dimensions_count > 0u)
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Cannot execute unary operations on arrays.",
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                 (long) input_expression->source_line)
        return KAN_FALSE;
    }

    switch (input_expression->unary_operation.operation)
    {
#define NEEDS_TO_READ(OPERATOR_STRING)                                                                                 \
    if (!is_access_readable (operand->output.access))                                                                  \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATOR_STRING                                                    \
                 "\" operation as operand must be readable, but it isn't.",                                            \
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,               \
                 (long) input_expression->source_line)                                                                 \
        return KAN_FALSE;                                                                                              \
    }

    case KAN_RPL_UNARY_OPERATION_NEGATE:
    {
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NEGATE;
        NEEDS_TO_READ ("-")

        if (operand->output.class != COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
            operand->output.class != COMPILER_INSTANCE_TYPE_CLASS_MATRIX)
        {
            KAN_LOG (
                rpl_compiler_context, KAN_LOG_ERROR,
                "[%s:%s:%s:%ld] Cannot apply \"-\" operation to type \"%s\", only vectors and matrices are supported.",
                context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                (long) input_expression->source_line, get_type_name_for_logging (&operand->output))
            return KAN_FALSE;
        }

        enum inbuilt_type_item_t item = operand->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR ?
                                            operand->output.vector_data->item :
                                            operand->output.matrix_data->item;

        switch (item)
        {
        case INBUILT_TYPE_ITEM_FLOAT:
        case INBUILT_TYPE_ITEM_SIGNED:
            break;

        case INBUILT_TYPE_ITEM_UNSIGNED:
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot apply \"-\" operation to type \"%s\" as unsigned types cannot be negated.",
                     context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line, get_type_name_for_logging (&operand->output))
            return KAN_FALSE;
        }

        copy_type_definition (&result_expression->output, &operand->output);
        result_expression->output.access = KAN_RPL_ACCESS_CLASS_READ_ONLY;
        result_expression->output.flags &= ~COMPILER_INSTANCE_TYPE_INTERFACE_POINTER;
        return KAN_TRUE;
    }

    case KAN_RPL_UNARY_OPERATION_NOT:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NOT;
        NEEDS_TO_READ ("!")

        if (operand->output.class != COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot apply \"!\" operation to non-boolean type \"%s\".", context->log_name,
                     resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line, get_type_name_for_logging (&operand->output))
            return KAN_FALSE;
        }

        result_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN;
        return KAN_TRUE;

    case KAN_RPL_UNARY_OPERATION_BITWISE_NOT:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_NOT;
        NEEDS_TO_READ ("~")

        if (operand->output.class != COMPILER_INSTANCE_TYPE_CLASS_VECTOR ||
            !inbuilt_type_item_is_integer (operand->output.vector_data->item) ||
            operand->output.vector_data->items_count > 1u)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot apply \"~\" operation to type \"%s\", only u1 and s1 is supported.",
                     context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line, get_type_name_for_logging (&operand->output))
            return KAN_FALSE;
        }

        result_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_VECTOR;
        result_expression->output.vector_data =
            &STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER, 1u)];
        return KAN_TRUE;

#undef NEEDS_TO_READ
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static kan_bool_t resolve_expression (struct rpl_compiler_context_t *context,
                                      struct rpl_compiler_instance_t *instance,
                                      struct kan_rpl_intermediate_t *intermediate,
                                      struct resolve_expression_scope_t *resolve_scope,
                                      struct kan_rpl_expression_t *expression,
                                      struct compiler_instance_expression_node_t **output)
{
    *output = NULL;
    if (expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    {
        return KAN_TRUE;
    }
    // We check conditional expressions before anything else as they have special allocation strategy.
    else if (expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE)
    {
        switch (evaluate_conditional (context, intermediate, expression->conditional_scope.condition_index, KAN_TRUE))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            return KAN_FALSE;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
            return resolve_expression (context, instance, intermediate, resolve_scope,
                                       &((struct kan_rpl_expression_t *) intermediate->expression_storage
                                             .data)[expression->conditional_scope.body_index],
                                       output);

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            return KAN_TRUE;
        }
    }
    else if (expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS)
    {
        switch (evaluate_conditional (context, intermediate, expression->conditional_alias.condition_index, KAN_TRUE))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            return KAN_FALSE;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
        {
            if (!check_alias_or_variable_name_is_not_occupied (context, instance, resolve_scope,
                                                               expression->conditional_alias.name))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to add alias \"%s\" as its name is already occupied by other active "
                         "alias in this scope.",
                         context->log_name, resolve_scope->function->module_name, expression->source_name,
                         (long) expression->source_line, expression->conditional_alias.name)
                return KAN_FALSE;
            }

            struct resolve_expression_alias_node_t *alias_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                &context->resolve_allocator, struct resolve_expression_alias_node_t);

            alias_node->name = expression->conditional_alias.name;
            if (!resolve_expression (context, instance, intermediate, resolve_scope,
                                     &((struct kan_rpl_expression_t *) intermediate->expression_storage
                                           .data)[expression->conditional_alias.expression_index],
                                     &alias_node->resolved_expression))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve alias \"%s\" internal expression.", context->log_name,
                         resolve_scope->function->module_name, expression->source_name, (long) expression->source_line,
                         expression->conditional_alias.name)
                return KAN_FALSE;
            }

            alias_node->next = resolve_scope->first_alias;
            resolve_scope->first_alias = alias_node;
            return KAN_TRUE;
        }

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            return KAN_TRUE;
        }
    }
    // If it is an alias: pre-resolve it without creating excessive nodes.
    else if (expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER)
    {
        struct resolve_expression_alias_node_t *alias = resolve_find_alias (resolve_scope, expression->identifier);
        if (alias)
        {
            *output = alias->resolved_expression;
            return KAN_TRUE;
        }
    }

    struct compiler_instance_expression_node_t *new_expression = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &instance->resolve_allocator, struct compiler_instance_expression_node_t);

    new_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_VOID;
    new_expression->output.array_size_runtime = KAN_FALSE;
    new_expression->output.array_dimensions_count = 0u;
    new_expression->output.array_dimensions = NULL;
    new_expression->output.access = KAN_RPL_ACCESS_CLASS_READ_ONLY;
    new_expression->output.flags = 0u;

    new_expression->module_name = resolve_scope->function->module_name;
    new_expression->source_name = expression->source_name;
    new_expression->source_line = expression->source_line;
    *output = new_expression;

    switch (expression->type)
    {
    case KAN_RPL_EXPRESSION_NODE_TYPE_NOPE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS:
        // Should've been processed earlier.
        KAN_ASSERT (KAN_FALSE)
        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER:
    {
        struct compiler_instance_buffer_node_t *buffer = instance->first_buffer;
        while (buffer)
        {
            if (buffer->name == expression->identifier)
            {
                new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE;
                new_expression->structured_buffer_reference = buffer;

                new_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_BUFFER;
                new_expression->output.buffer_data = buffer;

                switch (buffer->type)
                {
                case KAN_RPL_BUFFER_TYPE_UNIFORM:
                case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
                    new_expression->output.access = KAN_RPL_ACCESS_CLASS_READ_ONLY;
                    break;
                }

                new_expression->output.flags |= COMPILER_INSTANCE_TYPE_INTERFACE_POINTER;
                return resolve_use_buffer (context, instance, resolve_scope->function,
                                           resolve_scope->function->required_stage, buffer, expression->source_line);
            }

            buffer = buffer->next;
        }

        struct compiler_instance_sampler_node_t *sampler = instance->first_sampler;
        while (sampler)
        {
            if (sampler->name == expression->identifier)
            {
                new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_SAMPLER_REFERENCE;
                new_expression->sampler_reference = sampler;

                new_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_SAMPLER;
                new_expression->output.access = KAN_RPL_ACCESS_CLASS_READ_ONLY;

                return resolve_use_sampler (context, instance, resolve_scope->function, sampler,
                                            expression->source_line);
            }

            sampler = sampler->next;
        }

        struct compiler_instance_image_node_t *image = instance->first_image;
        while (image)
        {
            if (image->name == expression->identifier)
            {
                new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_REFERENCE;
                new_expression->image_reference = image;

                new_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_IMAGE;
                new_expression->output.image_type = image->type;
                new_expression->output.access = KAN_RPL_ACCESS_CLASS_READ_ONLY;

                if (image->array_size > 1u)
                {
                    new_expression->output.array_dimensions_count = 1u;
                    new_expression->output.array_dimensions = &image->array_size;
                }

                return resolve_use_image (context, instance, resolve_scope->function, image, expression->source_line);
            }

            image = image->next;
        }

        struct compiler_instance_scope_variable_item_t *variable =
            resolve_find_variable (resolve_scope, expression->identifier);

        if (variable)
        {
            new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_REFERENCE;
            new_expression->variable_reference = variable;
            copy_type_definition (&new_expression->output, &variable->variable->type);
            return KAN_TRUE;
        }

        // Search for option value that can be used here.
        for (kan_loop_size_t option_value_index = 0u; option_value_index < context->option_values.size;
             ++option_value_index)
        {
            struct rpl_compiler_context_option_value_t *value =
                &((struct rpl_compiler_context_option_value_t *) context->option_values.data)[option_value_index];

            if (value->name == expression->identifier)
            {
                switch (value->type)
                {
                case KAN_RPL_OPTION_TYPE_FLAG:
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Detected attempt to access flag option \"%s\" value outside of constant "
                             "expressions. It is forbidden as it can cause performance issues.",
                             context->log_name, resolve_scope->function->module_name, expression->source_name,
                             (long) expression->source_line, expression->identifier)
                    return KAN_FALSE;

                case KAN_RPL_OPTION_TYPE_COUNT:
                    new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_UNSIGNED_LITERAL;
                    new_expression->unsigned_literal = (kan_rpl_unsigned_int_literal_t) value->count_value;
                    new_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_VECTOR;
                    new_expression->output.vector_data =
                        &STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_UNSIGNED, 1u)];
                    return KAN_TRUE;
                }
            }
        }

        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Cannot resolve identifier \"%s\" to either variable or structured buffer access.",
                 context->log_name, resolve_scope->function->module_name, expression->source_name,
                 (long) expression->source_line, expression->identifier)
        return KAN_FALSE;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL:
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_FLOATING_LITERAL;
        new_expression->floating_literal = expression->floating_literal;
        new_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_VECTOR;
        new_expression->output.vector_data =
            &STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_FLOAT, 1u)];
        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_UNSIGNED_LITERAL:
        if (expression->unsigned_literal > UINT32_MAX)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Unsigned literal %lld is too big for some backends.", context->log_name,
                     resolve_scope->function->module_name, expression->source_name, (long) expression->source_line,
                     (long long) expression->unsigned_literal)
            return KAN_FALSE;
        }

        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_UNSIGNED_LITERAL;
        new_expression->unsigned_literal = expression->unsigned_literal;
        new_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_VECTOR;
        new_expression->output.vector_data =
            &STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_UNSIGNED, 1u)];
        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_SIGNED_LITERAL:
        if (expression->signed_literal < INT32_MIN || expression->signed_literal > INT32_MAX)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Signed literal %lld is too big for some backends.", context->log_name,
                     resolve_scope->function->module_name, expression->source_name, (long) expression->source_line,
                     (long long) expression->signed_literal)
            return KAN_FALSE;
        }

        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_SIGNED_LITERAL;
        new_expression->signed_literal = expression->signed_literal;
        new_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_VECTOR;
        new_expression->output.vector_data =
            &STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_SIGNED, 1u)];
        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_DECLARATION;
        kan_bool_t resolved = KAN_TRUE;

        if (!check_alias_or_variable_name_is_not_occupied (context, instance, resolve_scope,
                                                           expression->variable_declaration.variable_name))
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Failed to add variable \"%s\" as its name is already occupied in current scope.",
                     context->log_name, resolve_scope->function->module_name, expression->source_name,
                     (long) expression->source_line, expression->variable_declaration.variable_name)
            resolved = KAN_FALSE;
        }

        new_expression->variable_declaration.variable.name = expression->variable_declaration.variable_name;
        if (!resolve_type (context, instance, new_expression->module_name,
                           &new_expression->variable_declaration.variable.type,
                           expression->variable_declaration.type_name, expression->variable_declaration.variable_name,
                           expression->source_name, expression->source_line))
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_array_dimensions (context, instance, intermediate, &new_expression->variable_declaration.variable,
                                       KAN_FALSE, expression->variable_declaration.array_size_expression_list_size,
                                       expression->variable_declaration.array_size_expression_list_index, KAN_TRUE))
        {
            resolved = KAN_FALSE;
        }

        if (resolved)
        {
            switch (new_expression->variable_declaration.variable.type.class)
            {
            case COMPILER_INSTANCE_TYPE_CLASS_VOID:
            case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
            case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
                // While samplers and images are technically pointers that can be used, creating variables with them
                // can be tricky due to how logical pointers work in some target languages like SPIRV. Therefore,
                // they're only allowed as function parameters for now.
            case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
            case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Declaration \"%s\" has type \"%s\" which is not supported for variables.",
                         context->log_name, intermediate->log_name, expression->source_name,
                         (long) expression->source_line, expression->variable_declaration.variable_name,
                         expression->variable_declaration.type_name)
                resolved = KAN_FALSE;
                break;

            case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
            case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
            case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
                break;
            }
        }

        new_expression->variable_declaration.declared_in_scope = NULL;
        if (resolved)
        {
            new_expression->variable_declaration.variable.type.access = KAN_RPL_ACCESS_CLASS_READ_WRITE;
            new_expression->variable_declaration.variable.type.flags = 0u;

            struct resolve_expression_scope_t *owner_scope = resolve_scope;
            while (owner_scope && !owner_scope->associated_resolved_scope_if_any)
            {
                owner_scope = owner_scope->parent;
            }

            if (owner_scope)
            {
                struct compiler_instance_scope_variable_item_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                    &instance->resolve_allocator, struct compiler_instance_scope_variable_item_t);

                item->variable = &new_expression->variable_declaration.variable;
                item->next = owner_scope->associated_resolved_scope_if_any->scope.first_variable;

                owner_scope->associated_resolved_scope_if_any->scope.first_variable = item;
                new_expression->variable_declaration.declared_in_scope = item;
            }
            else
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Internal error: unable to register declared variable \"%s\" as there is no "
                         "suitable scope found.",
                         context->log_name, resolve_scope->function->module_name, expression->source_name,
                         (long) expression->source_line, expression->variable_declaration.variable_name)
                resolved = KAN_FALSE;
            }

            copy_type_definition (&new_expression->output, &new_expression->variable_declaration.variable.type);
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION:
        return resolve_binary_operation (context, instance, intermediate, resolve_scope, expression, new_expression);

    case KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION:
        return resolve_unary_operation (context, instance, intermediate, resolve_scope, expression, new_expression);

    case KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_SCOPE;
        new_expression->scope.first_variable = NULL;
        new_expression->scope.first_expression = NULL;
        new_expression->scope.leads_to_return = KAN_FALSE;
        new_expression->scope.leads_to_jump = KAN_FALSE;

        struct resolve_expression_scope_t child_scope = {
            .parent = resolve_scope,
            .function = resolve_scope->function,
            .first_alias = NULL,
            .associated_resolved_scope_if_any = new_expression,
            .associated_outer_loop_if_any = NULL,
        };

        kan_bool_t resolved = KAN_TRUE;
        struct compiler_instance_expression_list_item_t *last_expression = NULL;

        for (kan_loop_size_t index = 0u; index < expression->scope.statement_list_size; ++index)
        {
            const kan_rpl_size_t expression_index =
                ((kan_rpl_size_t *)
                     intermediate->expression_lists_storage.data)[expression->scope.statement_list_index + index];

            struct kan_rpl_expression_t *parser_expression =
                &((struct kan_rpl_expression_t *) intermediate->expression_storage.data)[expression_index];

            if (new_expression->scope.leads_to_return || new_expression->scope.leads_to_jump)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Found code after return/break/continue.",
                         context->log_name, resolve_scope->function->module_name, parser_expression->source_name,
                         (long) parser_expression->source_line)
                resolved = KAN_FALSE;
                break;
            }

            struct compiler_instance_expression_node_t *resolved_expression;

            if (resolve_expression (context, instance, intermediate, &child_scope, parser_expression,
                                    &resolved_expression))
            {
                // Expression will be null for inactive conditionals and for conditional aliases.
                if (resolved_expression)
                {
                    if (resolved_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_RETURN)
                    {
                        new_expression->scope.leads_to_return = KAN_TRUE;
                    }
                    else if (resolved_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_BREAK ||
                             resolved_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_CONTINUE)
                    {
                        new_expression->scope.leads_to_jump = KAN_TRUE;
                    }
                    else if (resolved_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_SCOPE)
                    {
                        new_expression->scope.leads_to_return = resolved_expression->scope.leads_to_return;
                        new_expression->scope.leads_to_jump = resolved_expression->scope.leads_to_jump;
                    }
                    else if (resolved_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_IF)
                    {
                        if (resolved_expression->if_.when_false)
                        {
                            new_expression->scope.leads_to_return =
                                resolved_expression->if_.when_true->scope.leads_to_return &&
                                resolved_expression->if_.when_false->scope.leads_to_return;
                            new_expression->scope.leads_to_jump =
                                resolved_expression->if_.when_true->scope.leads_to_jump &&
                                resolved_expression->if_.when_false->scope.leads_to_jump;
                        }
                    }

                    struct compiler_instance_expression_list_item_t *list_item =
                        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&instance->resolve_allocator,
                                                                  struct compiler_instance_expression_list_item_t);

                    list_item->next = NULL;
                    list_item->expression = resolved_expression;

                    if (last_expression)
                    {
                        last_expression->next = list_item;
                    }
                    else
                    {
                        new_expression->scope.first_expression = list_item;
                    }

                    last_expression = list_item;
                }
            }
            else
            {
                resolved = KAN_FALSE;
            }
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL:
    {
        if (expression->function_call.name == STATICS.sample_function_name ||
            expression->function_call.name == STATICS.sample_dref_function_name)
        {
            new_expression->type = expression->function_call.name == STATICS.sample_function_name ?
                                       COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_SAMPLE :
                                       COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_SAMPLE_DREF;

            if (expression->function_call.argument_list_size < 3u)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] \"%s\" call must have at least 3 arguments: sampler, image and at least "
                         "one image-specific argument.",
                         context->log_name, resolve_scope->function->module_name, expression->source_name,
                         (long) expression->source_line, expression->function_call.name)
                return KAN_FALSE;
            }

            const kan_rpl_size_t list_base_index = expression->function_call.argument_list_index;
            const kan_rpl_size_t sampler_expression_index =
                ((kan_rpl_size_t *) intermediate->expression_lists_storage.data)[list_base_index];

            if (!resolve_expression (
                    context, instance, intermediate, resolve_scope,
                    &((struct kan_rpl_expression_t *) intermediate->expression_storage.data)[sampler_expression_index],
                    &new_expression->image_sample.sampler))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve sampler argument expression for \"%s\" call.",
                         context->log_name, resolve_scope->function->module_name, expression->source_name,
                         (long) expression->source_line, expression->function_call.name)
                return KAN_FALSE;
            }

            // Must be always readable technically.
            KAN_ASSERT (is_access_readable (new_expression->image_sample.sampler->output.access))

            if (new_expression->image_sample.sampler->output.class != COMPILER_INSTANCE_TYPE_CLASS_SAMPLER ||
                new_expression->image_sample.sampler->output.array_size_runtime ||
                new_expression->image_sample.sampler->output.array_dimensions_count > 0u)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] First argument for \"%s\" call must be a single sampler, but it is not.",
                         context->log_name, resolve_scope->function->module_name, expression->source_name,
                         (long) expression->source_line, expression->function_call.name)
                return KAN_FALSE;
            }

            const kan_rpl_size_t image_expression_index =
                ((kan_rpl_size_t *) intermediate->expression_lists_storage.data)[list_base_index + 1u];

            if (!resolve_expression (
                    context, instance, intermediate, resolve_scope,
                    &((struct kan_rpl_expression_t *) intermediate->expression_storage.data)[image_expression_index],
                    &new_expression->image_sample.image))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve image argument expression for \"%s\" call.",
                         context->log_name, resolve_scope->function->module_name, expression->source_name,
                         (long) expression->source_line, expression->function_call.name)
                return KAN_FALSE;
            }

            // Must be always readable technically.
            KAN_ASSERT (is_access_readable (new_expression->image_sample.image->output.access))

            if (new_expression->image_sample.image->output.class != COMPILER_INSTANCE_TYPE_CLASS_IMAGE ||
                new_expression->image_sample.image->output.array_size_runtime ||
                new_expression->image_sample.image->output.array_dimensions_count > 0u)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Second argument for \"%s\" call must be a single image, but it is not.",
                         context->log_name, resolve_scope->function->module_name, expression->source_name,
                         (long) expression->source_line, expression->function_call.name)
                return KAN_FALSE;
            }

            struct compiler_instance_function_argument_node_t *image_specific_arguments = NULL;
            if (expression->function_call.name == STATICS.sample_function_name)
            {
                switch (new_expression->image_sample.image->output.image_type)
                {
                case KAN_RPL_IMAGE_TYPE_COLOR_2D:
                case KAN_RPL_IMAGE_TYPE_DEPTH_2D:
                    image_specific_arguments = STATICS.sample_2d_additional_arguments;
                    break;

                case KAN_RPL_IMAGE_TYPE_COLOR_3D:
                case KAN_RPL_IMAGE_TYPE_DEPTH_3D:
                    image_specific_arguments = STATICS.sample_3d_additional_arguments;
                    break;

                case KAN_RPL_IMAGE_TYPE_COLOR_CUBE:
                case KAN_RPL_IMAGE_TYPE_DEPTH_CUBE:
                    image_specific_arguments = STATICS.sample_cube_additional_arguments;
                    break;

                case KAN_RPL_IMAGE_TYPE_COLOR_2D_ARRAY:
                case KAN_RPL_IMAGE_TYPE_DEPTH_2D_ARRAY:
                    image_specific_arguments = STATICS.sample_2d_array_additional_arguments;
                    break;

                case KAN_RPL_IMAGE_TYPE_COUNT:
                    KAN_ASSERT (KAN_FALSE)
                    break;
                }
            }
            else
            {
                KAN_ASSERT (expression->function_call.name == STATICS.sample_dref_function_name)
                switch (new_expression->image_sample.image->output.image_type)
                {
                case KAN_RPL_IMAGE_TYPE_COLOR_2D:
                case KAN_RPL_IMAGE_TYPE_COLOR_3D:
                case KAN_RPL_IMAGE_TYPE_COLOR_CUBE:
                case KAN_RPL_IMAGE_TYPE_COLOR_2D_ARRAY:
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Caught \"%s\" for color image. Only depth images support dref sampling.",
                             context->log_name, resolve_scope->function->module_name, expression->source_name,
                             (long) expression->source_line, expression->function_call.name)
                    return KAN_FALSE;

                case KAN_RPL_IMAGE_TYPE_DEPTH_2D:
                    image_specific_arguments = STATICS.sample_dref_2d_additional_arguments;
                    break;

                case KAN_RPL_IMAGE_TYPE_DEPTH_3D:
                    image_specific_arguments = STATICS.sample_dref_3d_additional_arguments;
                    break;

                case KAN_RPL_IMAGE_TYPE_DEPTH_CUBE:
                    image_specific_arguments = STATICS.sample_dref_cube_additional_arguments;
                    break;

                case KAN_RPL_IMAGE_TYPE_DEPTH_2D_ARRAY:
                    image_specific_arguments = STATICS.sample_dref_2d_array_additional_arguments;
                    break;

                case KAN_RPL_IMAGE_TYPE_COUNT:
                    KAN_ASSERT (KAN_FALSE)
                    break;
                }
            }

            if (!resolve_expression_array_with_signature (context, instance, intermediate, resolve_scope,
                                                          new_expression, &new_expression->image_sample.first_argument,
                                                          expression->function_call.argument_list_size - 2u,
                                                          list_base_index + 2u, image_specific_arguments))
            {
                return KAN_FALSE;
            }

            new_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_VECTOR;
            new_expression->output.vector_data =
                &STATICS.vector_types[INBUILT_VECTOR_TYPE_INDEX (INBUILT_TYPE_ITEM_FLOAT, 4u)];
            return KAN_TRUE;
        }

        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_FUNCTION_CALL;
        kan_bool_t resolved = KAN_TRUE;
        new_expression->function_call.function = NULL;
        new_expression->function_call.first_argument = NULL;

        if (!resolve_function_by_name (context, instance, expression->function_call.name,
                                       resolve_scope->function->required_stage,
                                       &new_expression->function_call.function))
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Unable to find function \"%s\" for the call.",
                     context->log_name, resolve_scope->function->module_name, expression->source_name,
                     (long) expression->source_line, expression->function_call.name)
            resolved = KAN_FALSE;
        }
        else if (!resolve_expression_array_with_signature (
                     context, instance, intermediate, resolve_scope, new_expression,
                     &new_expression->function_call.first_argument, expression->function_call.argument_list_size,
                     expression->function_call.argument_list_index,
                     new_expression->function_call.function->first_argument))
        {
            resolved = KAN_FALSE;
        }
        else
        {
            copy_type_definition (&new_expression->output, &new_expression->function_call.function->return_type);
            // Cannot write to function return.
            new_expression->output.access = KAN_RPL_ACCESS_CLASS_READ_ONLY;
            // Result is always a function scope variable, never an interface pointer.
            new_expression->output.flags &= ~COMPILER_INSTANCE_TYPE_INTERFACE_POINTER;
        }

        // We need to pass callee accesses to the caller function.
        if (resolved)
        {
#define COPY_ACCESSES(TYPE)                                                                                            \
    struct compiler_instance_##TYPE##_access_node_t *TYPE##_access_node =                                              \
        new_expression->function_call.function->first_##TYPE##_access;                                                 \
                                                                                                                       \
    while (TYPE##_access_node)                                                                                         \
    {                                                                                                                  \
        struct compiler_instance_##TYPE##_access_node_t *existent_access_node =                                        \
            resolve_scope->function->first_##TYPE##_access;                                                            \
                                                                                                                       \
        while (existent_access_node)                                                                                   \
        {                                                                                                              \
            if (existent_access_node->TYPE == TYPE##_access_node->TYPE)                                                \
            {                                                                                                          \
                /* Already used, no need for further verification. */                                                  \
                break;                                                                                                 \
            }                                                                                                          \
                                                                                                                       \
            existent_access_node = existent_access_node->next;                                                         \
        }                                                                                                              \
                                                                                                                       \
        if (!existent_access_node)                                                                                     \
        {                                                                                                              \
            struct compiler_instance_##TYPE##_access_node_t *new_access_node =                                         \
                KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&instance->resolve_allocator,                                \
                                                          struct compiler_instance_##TYPE##_access_node_t);            \
                                                                                                                       \
            new_access_node->next = resolve_scope->function->first_##TYPE##_access;                                    \
            resolve_scope->function->first_##TYPE##_access = new_access_node;                                          \
            new_access_node->TYPE = TYPE##_access_node->TYPE;                                                          \
            new_access_node->direct_access_function = TYPE##_access_node->direct_access_function;                      \
        }                                                                                                              \
                                                                                                                       \
        TYPE##_access_node = TYPE##_access_node->next;                                                                 \
    }

            COPY_ACCESSES (container)
            COPY_ACCESSES (buffer)
            COPY_ACCESSES (sampler)
            COPY_ACCESSES (image)
#undef COPY_ACCESSES

            if (!resolve_scope->function->has_stage_specific_access &&
                new_expression->function_call.function->has_stage_specific_access)
            {
                resolve_scope->function->has_stage_specific_access = KAN_TRUE;
            }
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR:
    {
        // For any constructor, we need to resolve all the arguments first.
        // And only after that we can decide constructor type (for custom inbuilt constructors).

        kan_bool_t resolved = KAN_TRUE;
        struct compiler_instance_expression_list_item_t *first_expression = NULL;
        struct compiler_instance_expression_list_item_t *last_expression = NULL;

        for (kan_loop_size_t list_index = expression->constructor.argument_list_index;
             list_index < expression->constructor.argument_list_index + expression->constructor.argument_list_size;
             ++list_index)
        {
            struct compiler_instance_expression_node_t *resolved_expression;
            const kan_rpl_size_t expression_index =
                ((kan_rpl_size_t *) intermediate->expression_lists_storage.data)[list_index];

            if (resolve_expression (
                    context, instance, intermediate, resolve_scope,
                    &((struct kan_rpl_expression_t *) intermediate->expression_storage.data)[expression_index],
                    &resolved_expression))
            {
                if (!is_access_readable (resolved_expression->output.access))
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Constructor \"%s\" argument at index %u must be readable, but it isn't.",
                             context->log_name, new_expression->module_name, new_expression->source_name,
                             (long) new_expression->source_line, expression->constructor.type_name,
                             (unsigned int) (list_index - expression->constructor.argument_list_index))
                    return KAN_FALSE;
                }

                KAN_ASSERT (resolved_expression)
                struct compiler_instance_expression_list_item_t *list_item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                    &instance->resolve_allocator, struct compiler_instance_expression_list_item_t);

                list_item->next = NULL;
                list_item->expression = resolved_expression;

                if (last_expression)
                {
                    last_expression->next = list_item;
                }
                else
                {
                    first_expression = list_item;
                }

                last_expression = list_item;
            }
            else
            {
                resolved = KAN_FALSE;
            }
        }

        if (!resolved)
        {
            // Failed to resolve arguments.
            return KAN_FALSE;
        }

        struct inbuilt_vector_type_t *vector_type = NULL;
        struct inbuilt_matrix_type_t *matrix_type = NULL;
        struct compiler_instance_struct_node_t *struct_type = NULL;

        if ((vector_type = find_inbuilt_vector_type (expression->constructor.type_name)))
        {
            new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_VECTOR_CONSTRUCTOR;
            new_expression->vector_constructor.type = vector_type;
            new_expression->vector_constructor.first_argument = first_expression;
            new_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_VECTOR;
            new_expression->output.vector_data = vector_type;

            // Check special cases for one item constructors.
            if (expression->constructor.argument_list_size == 1u &&
                first_expression->expression->output.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR &&
                !first_expression->expression->output.array_size_runtime &&
                first_expression->expression->output.array_dimensions_count == 0u)
            {
                if (first_expression->expression->output.vector_data->items_count == vector_type->items_count)
                {
                    if (first_expression->expression->output.vector_data->item == vector_type->item)
                    {
                        new_expression->vector_constructor.variant = COMPILER_INSTANCE_VECTOR_CONSTRUCTOR_SKIP;
                    }
                    else
                    {
                        new_expression->vector_constructor.variant = COMPILER_INSTANCE_VECTOR_CONSTRUCTOR_CONVERT;
                    }

                    return KAN_TRUE;
                }
                else if (first_expression->expression->output.vector_data->items_count == 1u &&
                         first_expression->expression->output.vector_data->item == vector_type->item)
                {
                    new_expression->vector_constructor.variant = COMPILER_INSTANCE_VECTOR_CONSTRUCTOR_FILL;
                    return KAN_TRUE;
                }
            }

            // No more special cases, it can only be combine constructor.
            new_expression->vector_constructor.variant = COMPILER_INSTANCE_VECTOR_CONSTRUCTOR_COMBINE;
            kan_instance_size_t total_items = 0u;
            kan_instance_size_t argument_index = 0u;
            struct compiler_instance_expression_list_item_t *argument_expression = first_expression;

            while (argument_expression)
            {
                if (argument_expression->expression->output.array_size_runtime ||
                    argument_expression->expression->output.array_dimensions_count != 0u)
                {
                    KAN_LOG (
                        rpl_compiler_context, KAN_LOG_ERROR,
                        "[%s:%s:%s:%ld] Constructor \"%s\" argument at index %u is an array which is not supported.",
                        context->log_name, new_expression->module_name, new_expression->source_name,
                        (long) new_expression->source_line, expression->constructor.type_name,
                        (unsigned int) argument_index)
                    return KAN_FALSE;
                }

                if (argument_expression->expression->output.class != COMPILER_INSTANCE_TYPE_CLASS_VECTOR ||
                    argument_expression->expression->output.vector_data->item != vector_type->item)
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Constructor \"%s\" argument at index %u is not a vector with the same "
                             "item type.",
                             context->log_name, new_expression->module_name, new_expression->source_name,
                             (long) new_expression->source_line, expression->constructor.type_name,
                             (unsigned int) argument_index)
                    return KAN_FALSE;
                }

                total_items += argument_expression->expression->output.vector_data->items_count;
                ++argument_index;
                argument_expression = argument_expression->next;
            }

            if (total_items != vector_type->items_count)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Constructor \"%s\" arguments form %u components instead of %u components.",
                         context->log_name, new_expression->module_name, new_expression->source_name,
                         (long) new_expression->source_line, expression->constructor.type_name,
                         (unsigned int) total_items, (unsigned int) vector_type->items_count)
                return KAN_FALSE;
            }

            return KAN_TRUE;
        }
        else if ((matrix_type = find_inbuilt_matrix_type (expression->constructor.type_name)))
        {
            new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_MATRIX_CONSTRUCTOR;
            new_expression->matrix_constructor.type = matrix_type;
            new_expression->matrix_constructor.first_argument = first_expression;
            new_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_MATRIX;
            new_expression->output.matrix_data = matrix_type;

            // Check special cases for one item constructors.
            if (expression->constructor.argument_list_size == 1u &&
                first_expression->expression->output.class == COMPILER_INSTANCE_TYPE_CLASS_MATRIX &&
                !first_expression->expression->output.array_size_runtime &&
                first_expression->expression->output.array_dimensions_count == 0u)
            {
                struct inbuilt_matrix_type_t *argument_type = first_expression->expression->output.matrix_data;
                if (argument_type->item == matrix_type->item && argument_type->rows == matrix_type->rows &&
                    argument_type->columns == matrix_type->columns)
                {
                    new_expression->matrix_constructor.variant = COMPILER_INSTANCE_MATRIX_CONSTRUCTOR_SKIP;
                    return KAN_TRUE;
                }
                else if (argument_type->rows == matrix_type->rows && argument_type->columns == matrix_type->columns)
                {
                    new_expression->matrix_constructor.variant = COMPILER_INSTANCE_MATRIX_CONSTRUCTOR_CONVERT;
                    return KAN_TRUE;
                }
                else if (argument_type->item == matrix_type->item && argument_type->rows >= matrix_type->rows &&
                         argument_type->columns >= matrix_type->columns)
                {
                    new_expression->matrix_constructor.variant = COMPILER_INSTANCE_MATRIX_CONSTRUCTOR_CROP;
                    return KAN_TRUE;
                }
            }

            // No more special cases, it can only be combine constructor.
            new_expression->matrix_constructor.variant = COMPILER_INSTANCE_MATRIX_CONSTRUCTOR_COMBINE;

            if (expression->constructor.argument_list_size != matrix_type->columns)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Constructor \"%s\" received %u arguments, but matrix has %u columns.",
                         context->log_name, new_expression->module_name, new_expression->source_name,
                         (long) new_expression->source_line, expression->constructor.type_name,
                         (unsigned int) expression->constructor.argument_list_size, (unsigned int) matrix_type->columns)
                return KAN_FALSE;
            }

            kan_instance_size_t argument_index = 0u;
            struct compiler_instance_expression_list_item_t *argument_expression = first_expression;

            while (argument_expression)
            {
                if (argument_expression->expression->output.array_size_runtime ||
                    argument_expression->expression->output.array_dimensions_count != 0u)
                {
                    KAN_LOG (
                        rpl_compiler_context, KAN_LOG_ERROR,
                        "[%s:%s:%s:%ld] Constructor \"%s\" argument at index %u is an array which is not supported.",
                        context->log_name, new_expression->module_name, new_expression->source_name,
                        (long) new_expression->source_line, expression->constructor.type_name,
                        (unsigned int) argument_index)
                    return KAN_FALSE;
                }

                if (argument_expression->expression->output.class != COMPILER_INSTANCE_TYPE_CLASS_VECTOR ||
                    argument_expression->expression->output.vector_data->item != matrix_type->item ||
                    argument_expression->expression->output.vector_data->items_count != matrix_type->rows)
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Constructor \"%s\" argument at index %u is not a vector with same type as "
                             "matrix row.",
                             context->log_name, new_expression->module_name, new_expression->source_name,
                             (long) new_expression->source_line, expression->constructor.type_name,
                             (unsigned int) argument_index)
                    return KAN_FALSE;
                }

                ++argument_index;
                argument_expression = argument_expression->next;
            }

            return KAN_TRUE;
        }
        else if (!resolve_use_struct (context, instance, expression->constructor.type_name, &struct_type))
        {
            new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCT_CONSTRUCTOR;
            new_expression->struct_constructor.type = struct_type;
            new_expression->struct_constructor.first_argument = first_expression;
            new_expression->output.class = COMPILER_INSTANCE_TYPE_CLASS_STRUCT;
            new_expression->output.struct_data = struct_type;

            struct compiler_instance_declaration_node_t *declaration_node = struct_type->first_field;
            kan_instance_size_t argument_index = 0u;
            struct compiler_instance_expression_list_item_t *argument_expression = first_expression;

            while (argument_expression)
            {
                if (!declaration_node)
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Constructor \"%s\" has more arguments that structure has fields.",
                             context->log_name, new_expression->module_name, new_expression->source_name,
                             (long) new_expression->source_line, expression->constructor.type_name)
                    return KAN_FALSE;
                }

                struct compiler_instance_type_definition_t signature;
                copy_type_definition (&signature, &declaration_node->variable.type);
                signature.access = KAN_RPL_ACCESS_CLASS_READ_ONLY;
                signature.flags = 0u;

                if (!resolve_match_signature_at_index (context, resolve_scope->function->module_name, new_expression,
                                                       &signature, argument_index, argument_expression->expression))
                {
                    resolved = KAN_FALSE;
                }

                ++argument_index;
                argument_expression = argument_expression->next;
                declaration_node = declaration_node->next;
            }

            if (!resolved)
            {
                return KAN_FALSE;
            }

            if (declaration_node)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Constructor \"%s\" has less arguments that structure has fields.",
                         context->log_name, new_expression->module_name, new_expression->source_name,
                         (long) new_expression->source_line, expression->constructor.type_name)
                return KAN_FALSE;
            }

            return KAN_TRUE;
        }

        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Constructor \"%s\" type is unknown.",
                 context->log_name, new_expression->module_name, new_expression->source_name,
                 (long) new_expression->source_line, expression->constructor.type_name)
        return KAN_FALSE;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_IF:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_IF;
        kan_bool_t resolved = KAN_TRUE;

        if (resolve_expression (context, instance, intermediate, resolve_scope,
                                &((struct kan_rpl_expression_t *)
                                      intermediate->expression_storage.data)[expression->if_.condition_index],
                                &new_expression->if_.condition))
        {
            if (!is_access_readable (new_expression->if_.condition->output.access))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Condition of if is not readable.",
                         context->log_name, new_expression->module_name, new_expression->source_name,
                         (long) new_expression->source_line)
                resolved = KAN_FALSE;
            }

            if (new_expression->if_.condition->output.class != COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN ||
                new_expression->if_.condition->output.array_size_runtime ||
                new_expression->if_.condition->output.array_dimensions_count > 0u)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Condition of if cannot be resolved as boolean.", context->log_name,
                         new_expression->module_name, new_expression->source_name, (long) new_expression->source_line)
                resolved = KAN_FALSE;
            }
        }
        else
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_expression (
                context, instance, intermediate, resolve_scope,
                &((struct kan_rpl_expression_t *) intermediate->expression_storage.data)[expression->if_.true_index],
                &new_expression->if_.when_true))
        {
            resolved = KAN_FALSE;
        }

        if (expression->if_.false_index != KAN_RPL_EXPRESSION_INDEX_NONE)
        {
            if (!resolve_expression (context, instance, intermediate, resolve_scope,
                                     &((struct kan_rpl_expression_t *)
                                           intermediate->expression_storage.data)[expression->if_.false_index],
                                     &new_expression->if_.when_false))
            {
                resolved = KAN_FALSE;
            }
        }
        else
        {
            new_expression->if_.when_false = NULL;
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FOR:
    {
        // Loop must be inside scope to avoid leaking out init variable.
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_SCOPE;
        new_expression->scope.first_variable = NULL;
        new_expression->scope.leads_to_return = KAN_FALSE;
        new_expression->scope.leads_to_jump = KAN_FALSE;

        struct compiler_instance_expression_node_t *loop_expression = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &instance->resolve_allocator, struct compiler_instance_expression_node_t);

        loop_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_FOR;
        loop_expression->module_name = resolve_scope->function->module_name;
        loop_expression->source_name = expression->source_name;
        loop_expression->source_line = expression->source_line;

        struct compiler_instance_expression_list_item_t *list_item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &instance->resolve_allocator, struct compiler_instance_expression_list_item_t);

        list_item->next = NULL;
        list_item->expression = loop_expression;
        new_expression->scope.first_expression = list_item;
        kan_bool_t resolved = KAN_TRUE;

        struct resolve_expression_scope_t loop_init_scope = {
            .parent = resolve_scope,
            .function = resolve_scope->function,
            .first_alias = NULL,
            .associated_resolved_scope_if_any = new_expression,
            .associated_outer_loop_if_any = loop_expression,
        };

        if (!resolve_expression (
                context, instance, intermediate, &loop_init_scope,
                &((struct kan_rpl_expression_t *) intermediate->expression_storage.data)[expression->for_.init_index],
                &loop_expression->for_.init))
        {
            resolved = KAN_FALSE;
        }

        if (resolve_expression (context, instance, intermediate, &loop_init_scope,
                                &((struct kan_rpl_expression_t *)
                                      intermediate->expression_storage.data)[expression->for_.condition_index],
                                &loop_expression->for_.condition))
        {
            if (!is_access_readable (loop_expression->for_.condition->output.access))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Condition of for is not readable.",
                         context->log_name, new_expression->module_name, new_expression->source_name,
                         (long) new_expression->source_line)
                resolved = KAN_FALSE;
            }

            if (loop_expression->for_.condition->output.class != COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN ||
                loop_expression->for_.condition->output.array_size_runtime ||
                loop_expression->for_.condition->output.array_dimensions_count > 0u)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Condition of for cannot be resolved as boolean.", context->log_name,
                         loop_expression->module_name, loop_expression->source_name,
                         (long) loop_expression->source_line)
                resolved = KAN_FALSE;
            }
        }
        else
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_expression (
                context, instance, intermediate, &loop_init_scope,
                &((struct kan_rpl_expression_t *) intermediate->expression_storage.data)[expression->for_.step_index],
                &loop_expression->for_.step))
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_expression (
                context, instance, intermediate, &loop_init_scope,
                &((struct kan_rpl_expression_t *) intermediate->expression_storage.data)[expression->for_.body_index],
                &loop_expression->for_.body))
        {
            resolved = KAN_FALSE;
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_WHILE:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_WHILE;
        kan_bool_t resolved = KAN_TRUE;

        struct resolve_expression_scope_t while_loop_scope = {
            .parent = resolve_scope,
            .function = resolve_scope->function,
            .first_alias = NULL,
            .associated_resolved_scope_if_any = NULL,
            .associated_outer_loop_if_any = new_expression,
        };

        if (resolve_expression (context, instance, intermediate, &while_loop_scope,
                                &((struct kan_rpl_expression_t *)
                                      intermediate->expression_storage.data)[expression->while_.condition_index],
                                &new_expression->while_.condition))
        {
            if (!is_access_readable (new_expression->while_.condition->output.access))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Condition of while is not readable.",
                         context->log_name, new_expression->module_name, new_expression->source_name,
                         (long) new_expression->source_line)
                resolved = KAN_FALSE;
            }

            if (new_expression->while_.condition->output.class != COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN ||
                new_expression->while_.condition->output.array_size_runtime ||
                new_expression->while_.condition->output.array_dimensions_count > 0u)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Condition of while cannot be resolved as boolean.", context->log_name,
                         new_expression->module_name, new_expression->source_name, (long) new_expression->source_line)
                resolved = KAN_FALSE;
            }
        }
        else
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_expression (
                context, instance, intermediate, &while_loop_scope,
                &((struct kan_rpl_expression_t *) intermediate->expression_storage.data)[expression->while_.body_index],
                &new_expression->while_.body))
        {
            resolved = KAN_FALSE;
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_BREAK:
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_BREAK;
        new_expression->break_loop = resolve_find_loop_in_current_context (resolve_scope);

        if (!new_expression->break_loop)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Caught break without associated top level loop.", context->log_name,
                     new_expression->module_name, new_expression->source_name, (long) new_expression->source_line)
            return KAN_FALSE;
        }

        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONTINUE:
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_CONTINUE;
        new_expression->continue_loop = resolve_find_loop_in_current_context (resolve_scope);

        if (!new_expression->continue_loop)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Caught continue without associated top level loop.", context->log_name,
                     new_expression->module_name, new_expression->source_name, (long) new_expression->source_line)
            return KAN_FALSE;
        }

        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_RETURN:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_RETURN;
        kan_bool_t resolved = KAN_TRUE;

        if (expression->return_index != KAN_RPL_EXPRESSION_INDEX_NONE)
        {
            if (resolve_expression (
                    context, instance, intermediate, resolve_scope,
                    &((struct kan_rpl_expression_t *) intermediate->expression_storage.data)[expression->return_index],
                    &new_expression->return_expression))
            {
                if (resolve_scope->function->return_type.class == COMPILER_INSTANCE_TYPE_CLASS_VOID)
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Caught attempt to return \"%s\" from function \"%s\" which returns void.",
                             context->log_name, new_expression->module_name, new_expression->source_name,
                             (long) new_expression->source_line,
                             get_type_name_for_logging (&new_expression->return_expression->output),
                             resolve_scope->function->name)
                    resolved = KAN_FALSE;
                }

                if (new_expression->return_expression->output.array_size_runtime ||
                    new_expression->return_expression->output.array_dimensions_count > 0u)
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Caught return of array from function \"%s\" which is not supported.",
                             context->log_name, new_expression->module_name, new_expression->source_name,
                             (long) new_expression->source_line, resolve_scope->function->name)
                    resolved = KAN_FALSE;
                }

                if (!is_access_readable (new_expression->return_expression->output.access))
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Return expression result is not readable.", context->log_name,
                             new_expression->module_name, new_expression->source_name,
                             (long) new_expression->source_line)
                    resolved = KAN_FALSE;
                }

                if (!is_type_definition_base_equal (&resolve_scope->function->return_type,
                                                    &new_expression->return_expression->output))
                {
                    KAN_LOG (
                        rpl_compiler_context, KAN_LOG_ERROR,
                        "[%s:%s:%s:%ld] Caught attempt to return \"%s\" from function \"%s\" which returns \"%s\".",
                        context->log_name, new_expression->module_name, new_expression->source_name,
                        (long) new_expression->source_line,
                        get_type_name_for_logging (&new_expression->return_expression->output),
                        resolve_scope->function->name,
                        get_type_name_for_logging (&resolve_scope->function->return_type))
                    resolved = KAN_FALSE;
                }
            }
            else
            {
                resolved = KAN_FALSE;
            }
        }
        else
        {
            new_expression->return_expression = NULL;
            if (resolve_scope->function->return_type.class != COMPILER_INSTANCE_TYPE_CLASS_VOID)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Caught void return from function \"%s\" which returns \"%s\".",
                         context->log_name, new_expression->module_name, new_expression->source_name,
                         (long) new_expression->source_line, resolve_scope->function->name,
                         get_type_name_for_logging (&resolve_scope->function->return_type))
                resolved = KAN_FALSE;
            }
        }

        return resolved;
    }
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static kan_bool_t resolve_argument_declarations (struct rpl_compiler_context_t *context,
                                                 struct rpl_compiler_instance_t *instance,
                                                 struct kan_rpl_intermediate_t *intermediate,
                                                 struct kan_dynamic_array_t *declaration_array,
                                                 struct compiler_instance_function_argument_node_t **first_output)
{
    kan_bool_t result = KAN_TRUE;
    struct compiler_instance_function_argument_node_t *first = NULL;
    struct compiler_instance_function_argument_node_t *last = NULL;

    for (kan_loop_size_t declaration_index = 0u; declaration_index < declaration_array->size; ++declaration_index)
    {
        struct kan_rpl_function_argument_t *source_argument =
            &((struct kan_rpl_function_argument_t *) declaration_array->data)[declaration_index];

        switch (evaluate_conditional (context, intermediate, source_argument->conditional_index, KAN_FALSE))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            result = KAN_FALSE;
            break;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
        {
            struct compiler_instance_function_argument_node_t *target_argument =
                KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&instance->resolve_allocator,
                                                          struct compiler_instance_function_argument_node_t);

            target_argument->next = NULL;
            target_argument->variable.name = source_argument->name;

            if (!resolve_type (context, instance, intermediate->log_name, &target_argument->variable.type,
                               source_argument->type_name, source_argument->name, source_argument->source_name,
                               source_argument->source_line))
            {
                result = KAN_FALSE;
            }

            if (!resolve_array_dimensions (context, instance, intermediate, &target_argument->variable, KAN_FALSE,
                                           source_argument->array_size_expression_list_size,
                                           source_argument->array_size_expression_list_index, KAN_TRUE))
            {
                result = KAN_FALSE;
            }

            if (result)
            {
                target_argument->variable.type.access = source_argument->access;
                target_argument->variable.type.flags = 0u;

                switch (target_argument->variable.type.class)
                {
                case COMPILER_INSTANCE_TYPE_CLASS_VOID:
                case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
                case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Argument \"%s\" has type \"%s\" which is not supported for arguments.",
                             context->log_name, intermediate->log_name, source_argument->source_name,
                             (long) source_argument->source_line, source_argument->name, source_argument->type_name)
                    result = KAN_FALSE;
                    break;

                case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
                case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
                case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
                    break;

                case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
                case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
                    if (target_argument->variable.type.access != KAN_RPL_ACCESS_CLASS_READ_ONLY)
                    {
                        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                                 "[%s:%s:%s:%ld] Argument \"%s\" of type \"%s\" must be readonly due to its type.",
                                 context->log_name, intermediate->log_name, source_argument->source_name,
                                 (long) source_argument->source_line, source_argument->name, source_argument->type_name)
                        result = KAN_FALSE;
                    }

                    break;
                }
            }

            target_argument->module_name = intermediate->log_name;
            target_argument->source_name = source_argument->source_name;
            target_argument->source_line = source_argument->source_line;

            if (last)
            {
                last->next = target_argument;
                last = target_argument;
            }
            else
            {
                first = target_argument;
                last = target_argument;
            }

            break;
        }

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            break;
        }
    }

    if (result)
    {
        *first_output = first;
    }

    return result;
}

static kan_bool_t resolve_new_used_function (struct rpl_compiler_context_t *context,
                                             struct rpl_compiler_instance_t *instance,
                                             struct kan_rpl_intermediate_t *intermediate,
                                             struct kan_rpl_function_t *function,
                                             enum kan_rpl_pipeline_stage_t context_stage,
                                             struct compiler_instance_function_node_t **output_node)
{
    if (is_global_name_occupied (context, instance, function->name))
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Cannot resolve function \"%s\" as its global name is already occupied.",
                 context->log_name, intermediate->log_name, function->source_name, (long) function->source_line,
                 function->name)

        *output_node = NULL;
        return KAN_FALSE;
    }

    struct compiler_instance_function_node_t *function_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &instance->resolve_allocator, struct compiler_instance_function_node_t);
    *output_node = function_node;

    kan_bool_t resolved = KAN_TRUE;
    function_node->name = function->name;

    if (resolve_type (context, instance, intermediate->log_name, &function_node->return_type,
                      function->return_type_name, function_node->name, function_node->source_name,
                      function_node->source_line))
    {
        switch (function_node->return_type.class)
        {
        case COMPILER_INSTANCE_TYPE_CLASS_VOID:
        case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
        case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
        case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
            break;

        case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
        case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
        case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
        case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Function return type \"%s\" is not void, vector, matrix or struct. Other types "
                     "are not supported for return.",
                     context->log_name, intermediate->log_name, function->source_name, (long) function->source_line,
                     function->return_type_name)
            resolved = KAN_FALSE;
            break;
        }
    }
    else
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Function return type \"%s\" is unknown.",
                 context->log_name, intermediate->log_name, function->source_name, (long) function->source_line,
                 function->return_type_name)
        resolved = KAN_FALSE;
    }

    function_node->has_stage_specific_access = KAN_FALSE;
    function_node->required_stage = context_stage;
    function_node->first_container_access = NULL;
    function_node->first_buffer_access = NULL;
    function_node->first_sampler_access = NULL;
    function_node->first_image_access = NULL;

    function_node->module_name = intermediate->log_name;
    function_node->source_name = function->source_name;
    function_node->source_line = function->source_line;

    if (!resolve_argument_declarations (context, instance, intermediate, &function->arguments,
                                        &function_node->first_argument))
    {
        resolved = KAN_FALSE;
    }

    struct compiler_instance_function_argument_node_t *argument_declaration = function_node->first_argument;
    function_node->first_argument_variable = NULL;
    struct compiler_instance_scope_variable_item_t *last_argument_variable = NULL;

    while (argument_declaration)
    {
        struct compiler_instance_scope_variable_item_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &instance->resolve_allocator, struct compiler_instance_scope_variable_item_t);

        item->next = NULL;
        item->variable = &argument_declaration->variable;

        if (last_argument_variable)
        {
            last_argument_variable->next = item;
        }
        else
        {
            function_node->first_argument_variable = item;
        }

        last_argument_variable = item;
        argument_declaration = argument_declaration->next;
    }

    struct resolve_expression_scope_t root_scope = {
        .parent = NULL,
        .function = function_node,
        .first_alias = NULL,
        .associated_resolved_scope_if_any = NULL,
        .associated_outer_loop_if_any = NULL,
    };

    if (!resolve_expression (
            context, instance, intermediate, &root_scope,
            &((struct kan_rpl_expression_t *) intermediate->expression_storage.data)[function->body_index],
            &function_node->body))
    {
        resolved = KAN_FALSE;
    }

    if (function_node->return_type.class != COMPILER_INSTANCE_TYPE_CLASS_VOID)
    {
        if (!function_node->body->scope.leads_to_return)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Not all paths from function \"%s\" return value.", context->log_name,
                     intermediate->log_name, function->source_name, (long) function->source_line, function->name)
            resolved = KAN_FALSE;
        }
    }

    // Parser should not produce conditionals as function bodies anyway, so this should be impossible.
    KAN_ASSERT (function_node->body)

    function_node->next = NULL;
    if (instance->last_function)
    {
        instance->last_function->next = function_node;
    }
    else
    {
        instance->first_function = function_node;
    }

    instance->last_function = function_node;
    return resolved;
}

static inline kan_bool_t resolve_function_check_usability (struct rpl_compiler_context_t *context,
                                                           struct compiler_instance_function_node_t *function_node,
                                                           enum kan_rpl_pipeline_stage_t context_stage)
{
    if (function_node->has_stage_specific_access && function_node->required_stage != context_stage)
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s] Function \"%s\" accesses stage specific globals for stage \"%s\", but is also called "
                 "from stage \"%s\", which results in undefined behavior. Dumping list of accessed "
                 "stage-specific globals.",
                 context->log_name, function_node->module_name, function_node->name,
                 get_stage_name (function_node->required_stage), get_stage_name (context_stage))

        struct compiler_instance_container_access_node_t *container_access = function_node->first_container_access;
        while (container_access)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s] Function \"%s\" accesses container \"%s\" through call of function \"%s\".",
                     context->log_name, function_node->module_name, function_node->name,
                     container_access->container->name, container_access->direct_access_function->name)

            container_access = container_access->next;
        }

        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static kan_bool_t resolve_function_by_name (struct rpl_compiler_context_t *context,
                                            struct rpl_compiler_instance_t *instance,
                                            kan_interned_string_t function_name,
                                            enum kan_rpl_pipeline_stage_t context_stage,
                                            struct compiler_instance_function_node_t **output_node)
{
    // Check inbuilt functions first.
    switch (context->pipeline_type)
    {
    case KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC:
        if ((*output_node = find_builtin_function (function_name)))
        {
            return resolve_function_check_usability (context, *output_node, context_stage);
        }

        break;
    }

    *output_node = NULL;
    struct compiler_instance_function_node_t *function_node = instance->first_function;

    while (function_node)
    {
        if (function_node->name == function_name)
        {
            if (!resolve_function_check_usability (context, function_node, context_stage))
            {
                return KAN_FALSE;
            }

            // Already resolved and no stage conflict.
            *output_node = function_node;
            return KAN_TRUE;
        }

        function_node = function_node->next;
    }

    kan_bool_t result = KAN_TRUE;
    kan_bool_t resolved = KAN_FALSE;

    for (kan_loop_size_t intermediate_index = 0u; intermediate_index < context->modules.size; ++intermediate_index)
    {
        struct kan_rpl_intermediate_t *intermediate =
            ((struct kan_rpl_intermediate_t **) context->modules.data)[intermediate_index];

        for (kan_loop_size_t function_index = 0u; function_index < intermediate->functions.size; ++function_index)
        {
            struct kan_rpl_function_t *function =
                &((struct kan_rpl_function_t *) intermediate->functions.data)[function_index];

            if (function->name == function_name)
            {
                switch (evaluate_conditional (context, intermediate, function->conditional_index, KAN_TRUE))
                {
                case CONDITIONAL_EVALUATION_RESULT_FAILED:
                    result = KAN_FALSE;
                    break;

                case CONDITIONAL_EVALUATION_RESULT_TRUE:
                    if (resolved)
                    {
                        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                                 "[%s:%s] There are multiple active prototypes of function with name \"%s\".",
                                 context->log_name, intermediate->log_name, function_name)
                        result = KAN_FALSE;
                    }
                    else
                    {
                        if (!resolve_new_used_function (context, instance, intermediate, function, context_stage,
                                                        &function_node))
                        {
                            result = KAN_FALSE;
                        }

                        resolved = KAN_TRUE;
                    }

                    break;

                case CONDITIONAL_EVALUATION_RESULT_FALSE:
                    break;
                }
            }
        }
    }

    if (!resolved)
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s] Unable to find any active function \"%s\".",
                 context->log_name, function_name)
        return KAN_FALSE;
    }

    *output_node = function_node;
    return result;
}

kan_rpl_compiler_instance_t kan_rpl_compiler_context_resolve (kan_rpl_compiler_context_t compiler_context,
                                                              kan_instance_size_t entry_point_count,
                                                              struct kan_rpl_entry_point_t *entry_points)
{
    struct rpl_compiler_context_t *context = KAN_HANDLE_GET (compiler_context);
    struct rpl_compiler_instance_t *instance =
        kan_allocate_general (STATICS.rpl_compiler_instance_allocation_group, sizeof (struct rpl_compiler_instance_t),
                              _Alignof (struct rpl_compiler_instance_t));

    instance->pipeline_type = context->pipeline_type;
    instance->context_log_name = context->log_name;
    kan_stack_group_allocator_init (&instance->resolve_allocator, STATICS.rpl_compiler_instance_allocation_group,
                                    KAN_RPL_COMPILER_INSTANCE_RESOLVE_STACK);
    instance->entry_point_count = entry_point_count;

    if (instance->entry_point_count > 0u)
    {
        instance->entry_points = kan_stack_group_allocator_allocate (
            &instance->resolve_allocator, sizeof (struct kan_rpl_entry_point_t) * entry_point_count,
            _Alignof (struct kan_rpl_entry_point_t));
        memcpy (instance->entry_points, entry_points, sizeof (struct kan_rpl_entry_point_t) * entry_point_count);
    }
    else
    {
        instance->entry_points = NULL;
    }

    instance->first_setting = NULL;
    instance->last_setting = NULL;

    instance->first_struct = NULL;
    instance->last_struct = NULL;

    instance->first_container = NULL;
    instance->last_container = NULL;

    instance->first_buffer = NULL;
    instance->last_buffer = NULL;

    instance->first_sampler = NULL;
    instance->last_sampler = NULL;

    instance->first_image = NULL;
    instance->last_image = NULL;

    instance->first_function = NULL;
    instance->last_function = NULL;

    kan_bool_t successfully_resolved = KAN_TRUE;
    struct binding_location_assignment_counter_t assignment_counter = {
        .next_attribute_container_binding = 0u,
        .next_pass_set_binding = 0u,
        .next_material_set_binding = 0u,
        .next_object_set_binding = 0u,
        .next_shared_set_binding = 0u,
        .next_attribute_location = 0u,
        .next_state_location = 0u,
        .next_color_output_location = 0u,
    };

    for (kan_loop_size_t intermediate_index = 0u; intermediate_index < context->modules.size; ++intermediate_index)
    {
        struct kan_rpl_intermediate_t *intermediate =
            ((struct kan_rpl_intermediate_t **) context->modules.data)[intermediate_index];

        if (!resolve_settings (context, instance, intermediate, &intermediate->settings, KAN_TRUE,
                               &instance->first_setting, &instance->last_setting))
        {
            successfully_resolved = KAN_FALSE;
        }

        // Object that affect bindings and locations are always added
        // even if they're not used to preserve shader family compatibility.

        if (!resolve_containers (context, instance, intermediate, &assignment_counter))
        {
            successfully_resolved = KAN_FALSE;
        }

        if (!resolve_buffers (context, instance, intermediate, &assignment_counter))
        {
            successfully_resolved = KAN_FALSE;
        }

        if (!resolve_samplers (context, instance, intermediate, &assignment_counter))
        {
            successfully_resolved = KAN_FALSE;
        }

        if (!resolve_images (context, instance, intermediate, &assignment_counter))
        {
            successfully_resolved = KAN_FALSE;
        }
    }

    for (kan_loop_size_t entry_point_index = 0u; entry_point_index < entry_point_count; ++entry_point_index)
    {
        struct compiler_instance_function_node_t *mute;
        if (!resolve_function_by_name (context, instance, entry_points[entry_point_index].function_name,
                                       entry_points[entry_point_index].stage, &mute))
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s] Failed to resolve entry point at stage \"%s\" with function \"%s\".", context->log_name,
                     get_stage_name (entry_points[entry_point_index].stage),
                     entry_points[entry_point_index].function_name)
            successfully_resolved = KAN_FALSE;
        }
    }

    kan_stack_group_allocator_reset (&context->resolve_allocator);
    kan_rpl_compiler_instance_t handle = KAN_HANDLE_SET (kan_rpl_compiler_instance_t, instance);

    if (successfully_resolved)
    {
        // Here we would like to apply optimizations on compiled AST in future.
        return handle;
    }

    kan_rpl_compiler_instance_destroy (handle);
    return KAN_HANDLE_SET_INVALID (kan_rpl_compiler_instance_t);
}
