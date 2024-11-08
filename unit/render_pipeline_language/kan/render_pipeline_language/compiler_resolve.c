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
    CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER,
    CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING,
};

struct compile_time_evaluation_value_t
{
    enum compile_time_evaluation_value_type_t type;
    union
    {
        kan_bool_t boolean_value;
        int64_t integer_value;
        double floating_value;
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
        for (uint64_t option_index = 0u; option_index < instance->option_values.size; ++option_index)
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
                        if (option->count_value > INT64_MAX)
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
                            result.integer_value = (int64_t) option->count_value;
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

    case KAN_RPL_EXPRESSION_NODE_TYPE_INTEGER_LITERAL:
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER;
        result.integer_value = expression->integer_literal;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL:
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING;
        result.floating_value = expression->floating_literal;
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
        result.floating_value = (double) left_operand.integer_value OPERATOR right_operand.floating_value;             \
    }                                                                                                                  \
    else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING &&                                        \
             right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)                                          \
    {                                                                                                                  \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING;                                                      \
        result.floating_value = left_operand.floating_value OPERATOR (double) right_operand.integer_value;             \
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
        result.boolean_value = (double) left_operand.integer_value OPERATOR right_operand.floating_value;              \
    }                                                                                                                  \
    else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING &&                                        \
             right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)                                          \
    {                                                                                                                  \
        result.boolean_value = left_operand.floating_value OPERATOR (double) right_operand.integer_value;              \
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
                                                                  uint64_t conditional_index,
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
    for (uint64_t setting_index = 0u; setting_index < settings_array->size; ++setting_index)
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

static inline kan_bool_t resolve_array_dimensions (struct rpl_compiler_context_t *context,
                                                   struct rpl_compiler_instance_t *instance,
                                                   struct kan_rpl_intermediate_t *intermediate,
                                                   struct compiler_instance_variable_t *variable,
                                                   uint64_t dimensions_list_size,
                                                   uint64_t dimensions_list_index,
                                                   kan_bool_t instance_options_allowed)
{
    kan_bool_t result = KAN_TRUE;
    variable->type.array_dimensions_count = dimensions_list_size;

    if (variable->type.array_dimensions_count > 0u)
    {
        variable->type.array_dimensions = kan_stack_group_allocator_allocate (
            &instance->resolve_allocator, sizeof (uint64_t) * variable->type.array_dimensions_count,
            _Alignof (uint64_t));

        for (uint64_t dimension = 0u; dimension < variable->type.array_dimensions_count; ++dimension)
        {
            const uint64_t expression_index =
                ((uint64_t *) intermediate->expression_lists_storage.data)[dimensions_list_index + dimension];
            struct kan_rpl_expression_t *expression =
                &((struct kan_rpl_expression_t *) intermediate->expression_storage.data)[expression_index];

            struct compile_time_evaluation_value_t value =
                evaluate_compile_time_expression (context, intermediate, expression, instance_options_allowed);

            switch (value.type)
            {
            case CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR:
                result = KAN_FALSE;
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN:
            case CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING:
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Declaration \"%s\" array size at dimension %ld calculation resulted "
                         "in non-integer value.",
                         context->log_name, intermediate->log_name, expression->source_name,
                         (long) expression->source_line, variable->name, (long) dimension)
                result = KAN_FALSE;
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER:
                if (value.integer_value > 0u && value.integer_value <= UINT32_MAX)
                {
                    variable->type.array_dimensions[dimension] = (uint64_t) value.integer_value;
                }
                else
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Declaration \"%s\" array size at dimension %ld calculation resulted "
                             "in invalid value for array size %lld.",
                             context->log_name, intermediate->log_name, expression->source_name,
                             (long) expression->source_line, variable->name, (long) dimension,
                             (long long) value.integer_value)
                    result = KAN_FALSE;
                }
                break;
            }
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

static inline kan_bool_t resolve_variable_type (struct rpl_compiler_context_t *context,
                                                struct rpl_compiler_instance_t *instance,
                                                kan_interned_string_t intermediate_log_name,
                                                struct compiler_instance_variable_t *variable,
                                                kan_interned_string_t type_name,
                                                kan_interned_string_t declaration_name,
                                                kan_interned_string_t source_name,
                                                uint64_t source_line)
{
    variable->type.if_vector = NULL;
    variable->type.if_matrix = NULL;
    variable->type.if_struct = NULL;

    if (!(variable->type.if_vector = find_inbuilt_vector_type (type_name)) &&
        !(variable->type.if_matrix = find_inbuilt_matrix_type (type_name)) &&
        !resolve_use_struct (context, instance, type_name, &variable->type.if_struct))
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Declaration \"%s\" type \"%s\" is unknown.",
                 context->log_name, intermediate_log_name, source_name, (long) source_line, declaration_name, type_name)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static kan_bool_t resolve_declarations (struct rpl_compiler_context_t *context,
                                        struct rpl_compiler_instance_t *instance,
                                        struct kan_rpl_intermediate_t *intermediate,
                                        struct kan_dynamic_array_t *declaration_array,
                                        struct compiler_instance_declaration_node_t **first_output,
                                        kan_bool_t instance_options_allowed)
{
    kan_bool_t result = KAN_TRUE;
    uint64_t current_offset = 0u;

    struct compiler_instance_declaration_node_t *first = NULL;
    struct compiler_instance_declaration_node_t *last = NULL;

    for (uint64_t declaration_index = 0u; declaration_index < declaration_array->size; ++declaration_index)
    {
        struct kan_rpl_declaration_t *source_declaration =
            &((struct kan_rpl_declaration_t *) declaration_array->data)[declaration_index];

        switch (evaluate_conditional (context, intermediate, source_declaration->conditional_index,
                                      instance_options_allowed))
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
            target_declaration->variable.type.if_vector = NULL;
            target_declaration->variable.type.if_matrix = NULL;
            target_declaration->variable.type.if_struct = NULL;

            if (!resolve_variable_type (context, instance, intermediate->log_name, &target_declaration->variable,
                                        source_declaration->type_name, source_declaration->name,
                                        source_declaration->source_name, source_declaration->source_line))
            {
                result = KAN_FALSE;
            }

            if (!resolve_array_dimensions (context, instance, intermediate, &target_declaration->variable,
                                           source_declaration->array_size_expression_list_size,
                                           source_declaration->array_size_expression_list_index,
                                           instance_options_allowed))
            {
                result = KAN_FALSE;
            }

            if (result)
            {
                calculate_full_type_definition_size_and_alignment (
                    &target_declaration->variable.type, 0u, &target_declaration->size, &target_declaration->alignment);

                if (target_declaration->size != 0u && target_declaration->alignment != 0u)
                {
                    current_offset = kan_apply_alignment (current_offset, target_declaration->alignment);
                    target_declaration->offset = current_offset;
                    current_offset += target_declaration->size;
                }
            }

            target_declaration->meta_count = source_declaration->meta_list_size;
            if (target_declaration->meta_count > 0u)
            {
                target_declaration->meta = kan_stack_group_allocator_allocate (
                    &instance->resolve_allocator, sizeof (kan_interned_string_t) * target_declaration->meta_count,
                    _Alignof (kan_interned_string_t));
                memcpy (target_declaration->meta,
                        &((kan_interned_string_t *)
                              intermediate->meta_lists_storage.data)[source_declaration->meta_list_index],
                        sizeof (kan_interned_string_t) * target_declaration->meta_count);
            }
            else
            {
                target_declaration->meta = NULL;
            }

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

static kan_bool_t flatten_buffer_process_field (struct rpl_compiler_context_t *context,
                                                struct rpl_compiler_instance_t *instance,
                                                struct compiler_instance_buffer_node_t *buffer,
                                                struct compiler_instance_declaration_node_t *declaration,
                                                struct compiler_instance_buffer_flattening_graph_node_t *output_node,
                                                struct flattening_name_generation_buffer_t *name_generation_buffer,
                                                struct binding_location_assignment_counter_t *assignment_counter);

static kan_bool_t flatten_buffer_process_field_list (
    struct rpl_compiler_context_t *context,
    struct rpl_compiler_instance_t *instance,
    struct compiler_instance_buffer_node_t *buffer,
    struct compiler_instance_declaration_node_t *first_declaration,
    struct compiler_instance_buffer_flattening_graph_node_t *output_node,
    struct flattening_name_generation_buffer_t *name_generation_buffer,
    struct binding_location_assignment_counter_t *assignment_counter)
{
    kan_bool_t result = KAN_TRUE;
    struct compiler_instance_buffer_flattening_graph_node_t *last_root = NULL;
    struct compiler_instance_declaration_node_t *field = first_declaration;

    while (field)
    {
        struct compiler_instance_buffer_flattening_graph_node_t *new_root = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &instance->resolve_allocator, struct compiler_instance_buffer_flattening_graph_node_t);

        new_root->next_on_level = NULL;
        new_root->first_child = NULL;
        new_root->name = field->variable.name;
        new_root->flattened_result = NULL;

        const uint64_t length = name_generation_buffer->length;
        flattening_name_generation_buffer_append (name_generation_buffer, field->variable.name);

        if (!flatten_buffer_process_field (context, instance, buffer, field, new_root, name_generation_buffer,
                                           assignment_counter))
        {
            result = KAN_FALSE;
        }

        flattening_name_generation_buffer_reset (name_generation_buffer, length);
        if (last_root)
        {
            last_root->next_on_level = new_root;
        }
        else
        {
            if (output_node)
            {
                output_node->first_child = new_root;
            }
            else
            {
                buffer->flattening_graph_base = new_root;
            }
        }

        last_root = new_root;
        field = field->next;
    }

    return result;
}

static kan_bool_t flatten_buffer_process_field (struct rpl_compiler_context_t *context,
                                                struct rpl_compiler_instance_t *instance,
                                                struct compiler_instance_buffer_node_t *buffer,
                                                struct compiler_instance_declaration_node_t *declaration,
                                                struct compiler_instance_buffer_flattening_graph_node_t *output_node,
                                                struct flattening_name_generation_buffer_t *name_generation_buffer,
                                                struct binding_location_assignment_counter_t *assignment_counter)
{
    kan_bool_t result = KAN_TRUE;
    if (declaration->variable.type.if_vector || declaration->variable.type.if_matrix)
    {
        // Reached leaf.
        struct compiler_instance_buffer_flattened_declaration_t *flattened = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &instance->resolve_allocator, struct compiler_instance_buffer_flattened_declaration_t);

        flattened->next = NULL;
        flattened->source_declaration = declaration;
        flattened->readable_name = kan_string_intern (name_generation_buffer->buffer);

        switch (buffer->type)
        {
        case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
            flattened->location = assignment_counter->next_attribute_location;
            if (flattened->source_declaration->variable.type.if_vector)
            {
                ++assignment_counter->next_attribute_location;
            }
            else if (flattened->source_declaration->variable.type.if_matrix)
            {
                // Unfortunately, most graphic APIs cannot push matrix as one attribute,
                // therefore we need to process one matrix attribute as several column attributes.
                assignment_counter->next_attribute_location +=
                    flattened->source_declaration->variable.type.if_matrix->columns;
            }
            else
            {
                KAN_ASSERT (KAN_FALSE)
            }

            break;

        case KAN_RPL_BUFFER_TYPE_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
            KAN_ASSERT (KAN_FALSE)
            break;

        case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
            flattened->location = assignment_counter->next_vertex_output_location;
            ++assignment_counter->next_vertex_output_location;
            break;

        case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
            flattened->location = assignment_counter->next_fragment_output_location;
            ++assignment_counter->next_fragment_output_location;
            break;
        }

        if (buffer->last_flattened_declaration)
        {
            buffer->last_flattened_declaration->next = flattened;
        }
        else
        {
            buffer->first_flattened_declaration = flattened;
        }

        buffer->last_flattened_declaration = flattened;
        output_node->flattened_result = flattened;
    }
    else if (declaration->variable.type.if_struct)
    {
        if (!flatten_buffer_process_field_list (context, instance, buffer,
                                                declaration->variable.type.if_struct->first_field, output_node,
                                                name_generation_buffer, assignment_counter))
        {
            result = KAN_FALSE;
        }
    }

    return result;
}

static kan_bool_t flatten_buffer (struct rpl_compiler_context_t *context,
                                  struct rpl_compiler_instance_t *instance,
                                  struct compiler_instance_buffer_node_t *buffer,
                                  struct binding_location_assignment_counter_t *assignment_counter)
{
    kan_bool_t result = KAN_TRUE;
    struct flattening_name_generation_buffer_t name_generation_buffer;
    const uint64_t buffer_name_length = strlen (buffer->name);
    const uint64_t to_copy = KAN_MIN (KAN_RPL_COMPILER_INSTANCE_MAX_FLAT_NAME_LENGTH - 1u, buffer_name_length);

    flattening_name_generation_buffer_reset (&name_generation_buffer, to_copy);
    memcpy (name_generation_buffer.buffer, buffer->name, to_copy);

    if (!flatten_buffer_process_field_list (context, instance, buffer, buffer->first_field, NULL,
                                            &name_generation_buffer, assignment_counter))
    {
        result = KAN_FALSE;
    }

    return result;
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
        if (declaration->variable.type.if_vector)
        {
            const uint32_t size = declaration->variable.type.if_vector->items_count *
                                  inbuilt_type_item_size[declaration->variable.type.if_vector->item];

            if (size % 16u != 0u)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Declaration \"%s\" is found inside buffer \"%s\", but its size is not "
                         "multiple of 16, which is prone to cause errors when used with uniform buffers.",
                         context->log_name, declaration->module_name, declaration->source_name,
                         (long) declaration->source_line, declaration->variable.name, buffer->name)
                valid = KAN_FALSE;
            }
        }
        else if (declaration->variable.type.if_matrix)
        {
            const uint32_t size = declaration->variable.type.if_matrix->rows *
                                  declaration->variable.type.if_matrix->columns *
                                  inbuilt_type_item_size[declaration->variable.type.if_matrix->item];

            if (size % 16u != 0u)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Declaration \"%s\" is found inside buffer \"%s\", but its size is not "
                         "multiple of 16, which is prone to cause errors when used with uniform buffers.",
                         context->log_name, declaration->module_name, declaration->source_name,
                         (long) declaration->source_line, declaration->variable.name, buffer->name)
                valid = KAN_FALSE;
            }
        }
        else if (declaration->variable.type.if_struct)
        {
            if (!resolve_buffers_validate_uniform_internals_alignment (
                    context, buffer, declaration->variable.type.if_struct->first_field))
            {
                valid = KAN_FALSE;
            }
        }

        declaration = declaration->next;
    }

    return valid;
}

static kan_bool_t is_global_name_occupied (struct rpl_compiler_context_t *context,
                                           struct rpl_compiler_instance_t *instance,
                                           kan_interned_string_t name)
{
    struct compiler_instance_struct_node_t *struct_data = instance->first_struct;
    while (struct_data)
    {
        if (struct_data->name == name)
        {
            return KAN_TRUE;
        }

        struct_data = struct_data->next;
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

    struct compiler_instance_function_node_t *function = instance->first_function;
    while (function)
    {
        if (function->name == name)
        {
            return KAN_TRUE;
        }

        function = function->next;
    }

    for (uint64_t option_value_index = 0u; option_value_index < context->option_values.size; ++option_value_index)
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

static inline void calculate_size_and_alignment_from_declarations (
    struct compiler_instance_declaration_node_t *declaration, uint64_t *size_output, uint64_t *alignment_output)
{
    *size_output = 0u;
    *alignment_output = 1u;

    while (declaration)
    {
        *size_output = declaration->offset + declaration->size;
        *alignment_output = KAN_MAX (*alignment_output, declaration->alignment);
        declaration = declaration->next;
    }

    *size_output = kan_apply_alignment (*size_output, *alignment_output);
}

static kan_bool_t resolve_buffers (struct rpl_compiler_context_t *context,
                                   struct rpl_compiler_instance_t *instance,
                                   struct kan_rpl_intermediate_t *intermediate,
                                   struct binding_location_assignment_counter_t *assignment_counter)
{
    kan_bool_t result = KAN_TRUE;
    for (uint64_t buffer_index = 0u; buffer_index < intermediate->buffers.size; ++buffer_index)
    {
        struct kan_rpl_buffer_t *source_buffer =
            &((struct kan_rpl_buffer_t *) intermediate->buffers.data)[buffer_index];

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

#if defined(KAN_WITH_ASSERT)
            // Check that parser is not trying to assign sets to attribute buffers.
            if (target_buffer->type == KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE ||
                target_buffer->type == KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE)
            {
                KAN_ASSERT (target_buffer->set == KAN_RPL_SET_PASS)
            }
#endif

            if (!resolve_declarations (context, instance, intermediate, &source_buffer->fields,
                                       &target_buffer->first_field, KAN_FALSE))
            {
                result = KAN_FALSE;
            }

            switch (target_buffer->type)
            {
            case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
                target_buffer->binding = assignment_counter->next_attribute_buffer_binding;
                ++assignment_counter->next_attribute_buffer_binding;
                target_buffer->stable_binding = KAN_TRUE;
                break;

            case KAN_RPL_BUFFER_TYPE_UNIFORM:
            case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
                target_buffer->binding = assignment_counter->next_arbitrary_stable_buffer_binding;
                ++assignment_counter->next_arbitrary_stable_buffer_binding;
                target_buffer->stable_binding = KAN_TRUE;
                break;

            case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
                target_buffer->binding = assignment_counter->next_arbitrary_unstable_buffer_binding;
                ++assignment_counter->next_arbitrary_unstable_buffer_binding;
                target_buffer->stable_binding = KAN_FALSE;
                break;

            case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
            case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
                // Not an external buffers, so no binding.
                target_buffer->binding = INVALID_BINDING;
                target_buffer->stable_binding = KAN_FALSE;
                break;
            }

            target_buffer->flattening_graph_base = NULL;
            target_buffer->first_flattened_declaration = NULL;
            target_buffer->last_flattened_declaration = NULL;

            if (result)
            {
                switch (target_buffer->type)
                {
                case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
                case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
                {
                    flatten_buffer (context, instance, target_buffer, assignment_counter);
                    struct compiler_instance_buffer_flattened_declaration_t *declaration =
                        target_buffer->first_flattened_declaration;

                    while (declaration)
                    {
                        if (declaration->source_declaration->variable.type.array_dimensions_count > 0u)
                        {
                            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                                     "[%s:%s:%s:%ld] Attributes should not be arrays, but flattened declaration \"%s\" "
                                     "with array suffix found.",
                                     context->log_name, intermediate->log_name,
                                     declaration->source_declaration->source_name,
                                     (long) declaration->source_declaration->source_line, declaration->readable_name)
                            result = KAN_FALSE;
                        }

                        declaration = declaration->next;
                    }

                    break;
                }

                case KAN_RPL_BUFFER_TYPE_UNIFORM:
                case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
                    if (!resolve_buffers_validate_uniform_internals_alignment (context, target_buffer,
                                                                               target_buffer->first_field))
                    {
                        result = KAN_FALSE;
                    }

                    break;

                case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
                case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
                    break;

                case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
                    flatten_buffer (context, instance, target_buffer, assignment_counter);
                    break;

                case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
                {
                    flatten_buffer (context, instance, target_buffer, assignment_counter);
                    struct compiler_instance_buffer_flattened_declaration_t *declaration =
                        target_buffer->first_flattened_declaration;

                    while (declaration)
                    {
                        if (!declaration->source_declaration->variable.type.if_vector)
                        {
                            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                                     "[%s:%s:%s:%ld] Fragment stage output should only contain vector declarations, "
                                     "but flattened declaration \"%s\" with other type found.",
                                     context->log_name, intermediate->log_name,
                                     declaration->source_declaration->source_name,
                                     (long) declaration->source_declaration->source_line, declaration->readable_name)
                            result = KAN_FALSE;
                        }

                        declaration = declaration->next;
                    }

                    break;
                }
                }
            }

            calculate_size_and_alignment_from_declarations (target_buffer->first_field, &target_buffer->size,
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

static kan_bool_t resolve_samplers (struct rpl_compiler_context_t *context,
                                    struct rpl_compiler_instance_t *instance,
                                    struct kan_rpl_intermediate_t *intermediate,
                                    struct binding_location_assignment_counter_t *assignment_counter)
{
    kan_bool_t result = KAN_TRUE;
    for (uint64_t sampler_index = 0u; sampler_index < intermediate->samplers.size; ++sampler_index)
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
            target_sampler->type = source_sampler->type;

            target_sampler->used = KAN_FALSE;
            target_sampler->set = source_sampler->set;
            target_sampler->binding = assignment_counter->next_arbitrary_stable_buffer_binding;
            ++assignment_counter->next_arbitrary_stable_buffer_binding;

            struct compiler_instance_setting_node_t *first_setting = NULL;
            struct compiler_instance_setting_node_t *last_setting = NULL;

            if (!resolve_settings (context, instance, intermediate, &source_sampler->settings, KAN_FALSE,
                                   &first_setting, &last_setting))
            {
                result = KAN_FALSE;
            }

            target_sampler->first_setting = first_setting;
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

static kan_bool_t is_buffer_can_be_accessed_from_stage (struct compiler_instance_buffer_node_t *buffer,
                                                        enum kan_rpl_pipeline_stage_t stage,
                                                        kan_bool_t *output_unique_stage_binding,
                                                        kan_bool_t *used_as_output)
{
    *output_unique_stage_binding = KAN_FALSE;
    *used_as_output = KAN_FALSE;

    switch (buffer->type)
    {
    case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
    case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
    case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
    case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
        *output_unique_stage_binding = KAN_TRUE;
        return stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX;

    case KAN_RPL_BUFFER_TYPE_UNIFORM:
    case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
        return KAN_TRUE;

    case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
        *output_unique_stage_binding = KAN_TRUE;
        *used_as_output = stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX;
        return stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX ||
               stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT;

    case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
        *output_unique_stage_binding = KAN_TRUE;
        *used_as_output = stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT;
        return stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_TRUE;
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

    for (uint64_t intermediate_index = 0u; intermediate_index < context->modules.size; ++intermediate_index)
    {
        struct kan_rpl_intermediate_t *intermediate =
            ((struct kan_rpl_intermediate_t **) context->modules.data)[intermediate_index];

        for (uint64_t struct_index = 0u; struct_index < intermediate->structs.size; ++struct_index)
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

    if (!resolve_declarations (context, instance, selected_intermediate, &intermediate_struct->fields,
                               &struct_node->first_field, KAN_FALSE))
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

static inline kan_bool_t resolve_bind_function_to_stage (struct rpl_compiler_context_t *context,
                                                         struct compiler_instance_function_node_t *function,
                                                         enum kan_rpl_pipeline_stage_t stage,
                                                         uint64_t usage_line,
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

static kan_bool_t resolve_use_buffer (struct rpl_compiler_context_t *context,
                                      struct rpl_compiler_instance_t *instance,
                                      struct compiler_instance_function_node_t *function,
                                      uint64_t usage_line,
                                      enum kan_rpl_pipeline_stage_t stage,
                                      struct compiler_instance_buffer_node_t *buffer)
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

    kan_bool_t needs_binding;
    if (!is_buffer_can_be_accessed_from_stage (buffer, stage, &needs_binding, &access_node->used_as_output))
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Function \"%s\" is called in stage \"%s\" and tries to access buffer \"%s\" which is "
                 "not accessible in that stage.",
                 context->log_name, function->module_name, function->source_name, (long) usage_line, function->name,
                 get_stage_name (stage), buffer->name)
        return KAN_FALSE;
    }

    if (needs_binding)
    {
        if (!resolve_bind_function_to_stage (context, function, stage, usage_line, buffer->name))
        {
            return KAN_FALSE;
        }
    }

    buffer->used = KAN_TRUE;
    return KAN_TRUE;
}

static kan_bool_t resolve_use_sampler (struct rpl_compiler_context_t *context,
                                       struct rpl_compiler_instance_t *instance,
                                       struct compiler_instance_function_node_t *function,
                                       struct compiler_instance_sampler_node_t *sampler,
                                       uint64_t usage_line)
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

    enum kan_rpl_pipeline_stage_t sampling_stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT;
    switch (context->pipeline_type)
    {
    case KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC:
        sampling_stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT;
        break;
    }

    if (!resolve_bind_function_to_stage (context, function, sampling_stage, usage_line, sampler->name))
    {
        return KAN_FALSE;
    }

    sampler->used = KAN_TRUE;
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
    else if (owner_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_SAMPLER_CALL)
    {
        return owner_expression->sampler_call.sampler->name;
    }
    else if (owner_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_CONSTRUCTOR)
    {
        return get_type_name_for_logging (owner_expression->constructor.type_if_vector,
                                          owner_expression->constructor.type_if_matrix,
                                          owner_expression->constructor.type_if_struct);
    }

    return "<unknown_call_name>";
}

static inline kan_bool_t resolve_match_signature_at_index (struct rpl_compiler_context_t *context,
                                                           kan_interned_string_t module_name,
                                                           struct compiler_instance_expression_node_t *owner_expression,
                                                           struct compiler_instance_declaration_node_t *signature,
                                                           uint64_t signature_index,
                                                           struct compiler_instance_expression_node_t *expression)
{
    if (signature)
    {
        if ((signature->variable.type.if_vector &&
             signature->variable.type.if_vector != expression->output.type.if_vector) ||
            (signature->variable.type.if_matrix &&
             signature->variable.type.if_matrix != expression->output.type.if_matrix) ||
            (signature->variable.type.if_struct &&
             signature->variable.type.if_struct != expression->output.type.if_struct))
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Expression array item at index %ld for \"%s\" call has incorrect "
                     "type: \"%s\" while \"%s\" is expected.",
                     context->log_name, module_name, owner_expression->source_name,
                     (long) owner_expression->source_line, (long) signature_index,
                     get_expression_call_name_for_logging (owner_expression),
                     get_type_name_for_logging (expression->output.type.if_vector, expression->output.type.if_matrix,
                                                expression->output.type.if_struct),
                     get_type_name_for_logging (signature->variable.type.if_vector, signature->variable.type.if_matrix,
                                                signature->variable.type.if_struct))
            return KAN_FALSE;
        }
        else if (signature->variable.type.array_dimensions_count != expression->output.type.array_dimensions_count)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Expression array item at index %ld for \"%s\" call has incorrect "
                     "array dimension count: %ld while %ld is expected",
                     context->log_name, module_name, owner_expression->source_name,
                     (long) owner_expression->source_line, (long) signature_index,
                     get_expression_call_name_for_logging (owner_expression),
                     (long) expression->output.type.array_dimensions_count,
                     (long) signature->variable.type.array_dimensions_count)
            return KAN_FALSE;
        }
        else
        {
            for (uint64_t array_dimension_index = 0u;
                 array_dimension_index < signature->variable.type.array_dimensions_count; ++array_dimension_index)
            {
                if (signature->variable.type.array_dimensions[array_dimension_index] !=
                    expression->output.type.array_dimensions[array_dimension_index])
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Expression array item at index %ld for \"%s\" call has "
                             "incorrect array dimension %ld size: %ld while %ld is expected",
                             context->log_name, module_name, owner_expression->source_name,
                             (long) owner_expression->source_line, (long) signature_index,
                             get_expression_call_name_for_logging (owner_expression), (long) array_dimension_index,
                             (long) expression->output.type.array_dimensions[array_dimension_index],
                             (long) signature->variable.type.array_dimensions[array_dimension_index])
                    return KAN_FALSE;
                }
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
    uint64_t expression_list_size,
    uint64_t expression_list_index,
    struct compiler_instance_declaration_node_t *first_argument)
{
    kan_bool_t resolved = KAN_TRUE;
    struct compiler_instance_expression_list_item_t *last_expression = NULL;
    struct compiler_instance_declaration_node_t *current_argument = first_argument;
    uint64_t current_argument_index = 0u;

    for (uint64_t index = 0u; index < expression_list_size; ++index)
    {
        struct compiler_instance_expression_node_t *resolved_expression;
        const uint64_t expression_index =
            ((uint64_t *) intermediate->expression_lists_storage.data)[expression_list_index + index];

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
                                                   current_argument, current_argument_index, resolved_expression))
            {
                resolved = KAN_FALSE;
            }

            current_argument = current_argument->next;
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

static kan_bool_t is_buffer_writable_for_stage (struct compiler_instance_buffer_node_t *buffer,
                                                enum kan_rpl_pipeline_stage_t stage)
{
    switch (buffer->type)
    {
    case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
    case KAN_RPL_BUFFER_TYPE_UNIFORM:
    case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
    case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
    case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
    case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
        return KAN_FALSE;

    case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
        return stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX;

    case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
        return stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t resolve_field_access_ascend_flattened_buffer (
    struct rpl_compiler_context_t *context,
    struct rpl_compiler_instance_t *instance,
    struct resolve_expression_scope_t *resolve_scope,
    uint64_t stop_expression_line,
    struct compiler_instance_buffer_node_t *buffer,
    struct resolve_fiend_access_linear_node_t *chain_first,
    struct compiler_instance_buffer_flattened_declaration_t **declaration_output,
    struct resolve_fiend_access_linear_node_t **access_resolve_next_node)
{
    KAN_ASSERT (chain_first)
    *declaration_output = NULL;
    *access_resolve_next_node = NULL;

    if (!resolve_use_buffer (context, instance, resolve_scope->function, stop_expression_line,
                             resolve_scope->function->required_stage, buffer))
    {
        return KAN_FALSE;
    }

    struct resolve_fiend_access_linear_node_t *chain_current = chain_first;
    struct compiler_instance_buffer_flattening_graph_node_t *graph_node = buffer->flattening_graph_base;

    while (chain_current)
    {
        while (graph_node)
        {
            if (graph_node->name == chain_current->field_source->identifier)
            {
                break;
            }

            graph_node = graph_node->next_on_level;
        }

        if (!graph_node)
        {
            KAN_LOG (
                rpl_compiler_context, KAN_LOG_ERROR,
                "[%s:%s:%s:%ld] Failed to resolve flattened buffer access at field \"%s\": no path for such field.",
                context->log_name, resolve_scope->function->module_name, chain_current->field_source->source_name,
                (long) chain_current->field_source->source_line, chain_current->field_source->identifier)
            return KAN_FALSE;
        }

        if (graph_node->flattened_result)
        {
            *declaration_output = graph_node->flattened_result;
            *access_resolve_next_node = chain_current->next;
            return KAN_TRUE;
        }

        chain_current = chain_current->next;
        graph_node = graph_node->first_child;
    }

    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
             "[%s:%s:%s:%ld] Failed to resolve flattened buffer access: it didn't lead to concrete leaf field.",
             context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
             (long) chain_first->field_source->source_line)
    return KAN_FALSE;
}

static inline kan_bool_t resolve_field_access_structured (struct rpl_compiler_context_t *context,
                                                          struct rpl_compiler_instance_t *instance,
                                                          struct resolve_expression_scope_t *resolve_scope,
                                                          struct compiler_instance_expression_node_t *input_node,
                                                          struct resolve_fiend_access_linear_node_t *chain_first,
                                                          uint64_t chain_length,
                                                          struct compiler_instance_expression_node_t *result_expression)
{
    if (input_node->output.type.array_dimensions_count > 0u)
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Failed to resolve structured access: attempted to use \".\" on array.",
                 context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
                 (long) chain_first->field_source->source_line)
        return KAN_FALSE;
    }

    if (input_node->output.boolean)
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Failed to resolve structured access: attempted to use \".\" on boolean.",
                 context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
                 (long) chain_first->field_source->source_line)
        return KAN_FALSE;
    }

    result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS;
    result_expression->structured_access.input = input_node;
    result_expression->structured_access.access_chain_length = chain_length;
    result_expression->structured_access.access_chain_indices = kan_stack_group_allocator_allocate (
        &instance->resolve_allocator, sizeof (uint64_t) * chain_length, _Alignof (uint64_t));

    uint64_t index = 0u;
    result_expression->output.type.if_vector = input_node->output.type.if_vector;
    result_expression->output.type.if_matrix = input_node->output.type.if_matrix;
    result_expression->output.type.if_struct = input_node->output.type.if_struct;
    result_expression->output.type.array_dimensions_count = 0u;
    result_expression->output.type.array_dimensions = NULL;
    result_expression->output.boolean = KAN_FALSE;
    result_expression->output.writable = input_node->output.writable;

    if (input_node->type == COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE)
    {
        result_expression->output.writable = is_buffer_writable_for_stage (input_node->structured_buffer_reference,
                                                                           resolve_scope->function->required_stage);
    }

    struct resolve_fiend_access_linear_node_t *chain_current = chain_first;
    while (chain_current)
    {
        kan_bool_t found = KAN_FALSE;
        if (result_expression->output.type.array_dimensions_count > 0u)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Failed to resolve structured access: attempted to use \".\" on array.",
                     context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
                     (long) chain_first->field_source->source_line)
            return KAN_FALSE;
        }

        if (result_expression->output.type.if_vector)
        {
            if (result_expression->output.type.if_vector->items_count == 1u)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve structured access: \"%s\" treated as scalar type and "
                         "therefore has no fields.",
                         context->log_name, resolve_scope->function->module_name,
                         chain_first->field_source->source_name, (long) chain_first->field_source->source_line,
                         result_expression->output.type.if_vector->name)
                return KAN_FALSE;
            }

            if (chain_current->field_source->identifier[0u] != '_' ||
                chain_current->field_source->identifier[1u] < '0' ||
                chain_current->field_source->identifier[1u] > '9' ||
                chain_current->field_source->identifier[2u] != '\0')
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve structured access: vector item access specifier \".\" "
                         "doesn't match format \"_<digit>\".",
                         context->log_name, resolve_scope->function->module_name,
                         chain_first->field_source->source_name, (long) chain_first->field_source->source_line)
                return KAN_FALSE;
            }

            result_expression->structured_access.access_chain_indices[index] =
                chain_current->field_source->identifier[1u] - '0';

            if (result_expression->structured_access.access_chain_indices[index] >=
                result_expression->output.type.if_vector->items_count)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve structured access: \"%s\" has only %ld items, but item at "
                         "index %ld requested.",
                         context->log_name, resolve_scope->function->module_name,
                         chain_first->field_source->source_name, (long) chain_first->field_source->source_line,
                         result_expression->output.type.if_vector->name,
                         (long) result_expression->output.type.if_vector->items_count,
                         (long) result_expression->structured_access.access_chain_indices[index])
                return KAN_FALSE;
            }

            found = KAN_TRUE;
            switch (result_expression->output.type.if_vector->item)
            {
            case INBUILT_TYPE_ITEM_FLOAT:
                result_expression->output.type.if_vector = &STATICS.type_f1;
                break;
            case INBUILT_TYPE_ITEM_INTEGER:
                result_expression->output.type.if_vector = &STATICS.type_i1;
                break;
            }
        }
        else if (result_expression->output.type.if_matrix)
        {
            if (chain_current->field_source->identifier[0u] != '_' ||
                chain_current->field_source->identifier[1u] < '0' ||
                chain_current->field_source->identifier[1u] > '9' ||
                chain_current->field_source->identifier[2u] != '\0')
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve structured access: matrix column access specifier \".\" "
                         "doesn't match format \"_<digit>\".",
                         context->log_name, resolve_scope->function->module_name,
                         chain_first->field_source->source_name, (long) chain_first->field_source->source_line)
                return KAN_FALSE;
            }

            result_expression->structured_access.access_chain_indices[index] =
                chain_current->field_source->identifier[1u] - '0';

            if (result_expression->structured_access.access_chain_indices[index] >=
                result_expression->output.type.if_matrix->columns)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve structured access: \"%s\" has only %ld columns, but column "
                         "at index %ld requested.",
                         context->log_name, resolve_scope->function->module_name,
                         chain_first->field_source->source_name, (long) chain_first->field_source->source_line,
                         result_expression->output.type.if_matrix->name,
                         (long) result_expression->output.type.if_matrix->columns,
                         (long) result_expression->structured_access.access_chain_indices[index])
                return KAN_FALSE;
            }

            found = KAN_TRUE;
            switch (result_expression->output.type.if_matrix->item)
            {
            case INBUILT_TYPE_ITEM_FLOAT:
                result_expression->output.type.if_vector =
                    STATICS.floating_vector_types[result_expression->output.type.if_matrix->rows - 1u];
                break;
            case INBUILT_TYPE_ITEM_INTEGER:
                result_expression->output.type.if_vector =
                    STATICS.integer_vector_types[result_expression->output.type.if_matrix->rows - 1u];
                break;
            }

            result_expression->output.type.if_matrix = NULL;
        }

#define SEARCH_USING_DECLARATION                                                                                       \
    result_expression->structured_access.access_chain_indices[index] = 0u;                                             \
    while (declaration)                                                                                                \
    {                                                                                                                  \
        if (declaration->variable.name == chain_current->field_source->identifier)                                     \
        {                                                                                                              \
            found = KAN_TRUE;                                                                                          \
            result_expression->output.type.if_vector = declaration->variable.type.if_vector;                           \
            result_expression->output.type.if_matrix = declaration->variable.type.if_matrix;                           \
            result_expression->output.type.if_struct = declaration->variable.type.if_struct;                           \
            result_expression->output.type.array_dimensions_count = declaration->variable.type.array_dimensions_count; \
            result_expression->output.type.array_dimensions = declaration->variable.type.array_dimensions;             \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        ++result_expression->structured_access.access_chain_indices[index];                                            \
        declaration = declaration->next;                                                                               \
    }

        else if (result_expression->output.type.if_struct)
        {
            struct compiler_instance_declaration_node_t *declaration =
                result_expression->output.type.if_struct->first_field;
            SEARCH_USING_DECLARATION
        }
        else if (chain_current == chain_first &&
                 input_node->type == COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE)
        {
            struct compiler_instance_declaration_node_t *declaration =
                input_node->structured_buffer_reference->first_field;
            SEARCH_USING_DECLARATION
        }

#undef SEARCH_USING_DECLARATION

        if (!found)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Failed to resolve structured access at field \"%s\": no path for such field.",
                     context->log_name, resolve_scope->function->module_name, chain_current->field_source->source_name,
                     (long) chain_current->field_source->source_line, chain_current->field_source->identifier)
            return KAN_FALSE;
        }

        ++index;
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

        // If chain stop points to flattened buffer, we must resolve flattened buffer access first.
        if (chain_stop_expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER)
        {
            struct compiler_instance_buffer_node_t *buffer = instance->first_buffer;
            while (buffer)
            {
                if (buffer->name == chain_stop_expression->identifier)
                {
                    if (buffer->first_flattened_declaration)
                    {
                        struct compiler_instance_buffer_flattened_declaration_t *flattened_declaration;
                        if (!resolve_field_access_ascend_flattened_buffer (
                                context, instance, resolve_scope, chain_stop_expression->source_line, buffer,
                                chain_first, &flattened_declaration, &chain_first))
                        {
                            return KAN_FALSE;
                        }

                        if (chain_first)
                        {
                            chain_input_expression = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                                &instance->resolve_allocator, struct compiler_instance_expression_node_t);

                            const kan_bool_t writable =
                                is_buffer_writable_for_stage (buffer, resolve_scope->function->required_stage);

                            chain_input_expression->type =
                                writable ? COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS_OUTPUT :
                                           COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS_INPUT;

                            chain_input_expression->flattened_buffer_access = flattened_declaration;
                            chain_input_expression->module_name = resolve_scope->function->module_name;
                            chain_input_expression->source_name = chain_stop_expression->source_name;
                            chain_input_expression->source_line = chain_stop_expression->source_line;

                            chain_input_expression->output.type.if_vector =
                                flattened_declaration->source_declaration->variable.type.if_vector;
                            chain_input_expression->output.type.if_matrix =
                                flattened_declaration->source_declaration->variable.type.if_matrix;
                            chain_input_expression->output.type.if_struct =
                                flattened_declaration->source_declaration->variable.type.if_struct;
                            chain_input_expression->output.type.array_dimensions_count =
                                flattened_declaration->source_declaration->variable.type.array_dimensions_count;
                            chain_input_expression->output.type.array_dimensions =
                                flattened_declaration->source_declaration->variable.type.array_dimensions;
                            chain_input_expression->output.boolean = KAN_FALSE;
                            chain_input_expression->output.writable = writable;
                        }
                        else
                        {
                            // Full access chain was resolved as flattened access.
                            const kan_bool_t writable =
                                is_buffer_writable_for_stage (buffer, resolve_scope->function->required_stage);

                            result_expression->type =
                                writable ? COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS_OUTPUT :
                                           COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS_INPUT;

                            result_expression->flattened_buffer_access = flattened_declaration;
                            result_expression->output.type.if_vector =
                                flattened_declaration->source_declaration->variable.type.if_vector;
                            result_expression->output.type.if_matrix =
                                flattened_declaration->source_declaration->variable.type.if_matrix;
                            result_expression->output.type.if_struct =
                                flattened_declaration->source_declaration->variable.type.if_struct;
                            result_expression->output.type.array_dimensions_count =
                                flattened_declaration->source_declaration->variable.type.array_dimensions_count;
                            result_expression->output.type.array_dimensions =
                                flattened_declaration->source_declaration->variable.type.array_dimensions;
                            result_expression->output.boolean = KAN_FALSE;
                            result_expression->output.writable = writable;
                            return KAN_TRUE;
                        }
                    }

                    break;
                }

                buffer = buffer->next;
            }
        }

        if (!chain_input_expression && !resolve_expression (context, instance, intermediate, resolve_scope,
                                                            chain_stop_expression, &chain_input_expression))
        {
            return KAN_FALSE;
        }

        uint64_t chain_length = 0u;
        struct resolve_fiend_access_linear_node_t *chain_item = chain_first;

        while (chain_item)
        {
            ++chain_length;
            chain_item = chain_item->next;
        }

        return resolve_field_access_structured (context, instance, resolve_scope, chain_input_expression, chain_first,
                                                chain_length, result_expression);
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

    case KAN_RPL_BINARY_OPERATION_ARRAY_ACCESS:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ARRAY_INDEX;
        if (left->output.type.array_dimensions_count == 0u)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot execute array access as left operand in not an array.", context->log_name,
                     resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line)
            return KAN_FALSE;
        }

        if (right->output.type.if_vector != &STATICS.type_i1)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot execute array access as right operand is \"%s\" instead of i1.",
                     context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line,
                     get_type_name_for_logging (right->output.type.if_vector, right->output.type.if_matrix,
                                                right->output.type.if_struct))
            return KAN_FALSE;
        }

        result_expression->output.type.if_vector = left->output.type.if_vector;
        result_expression->output.type.if_matrix = left->output.type.if_matrix;
        result_expression->output.type.if_struct = left->output.type.if_struct;
        result_expression->output.boolean = left->output.boolean;
        result_expression->output.writable = left->output.writable;
        result_expression->output.type.array_dimensions_count = left->output.type.array_dimensions_count - 1u;
        result_expression->output.type.array_dimensions = left->output.type.array_dimensions + 1u;
        return KAN_TRUE;

#define CANNOT_EXECUTE_ON_ARRAYS(OPERATOR_STRING)                                                                      \
    if (left->output.type.array_dimensions_count != 0u || right->output.type.array_dimensions_count != 0u)             \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATOR_STRING "\" operation on arrays.", context->log_name,      \
                 resolve_scope->function->module_name, input_expression->source_name,                                  \
                 (long) input_expression->source_line)                                                                 \
        return KAN_FALSE;                                                                                              \
    }

#define CAN_ONLY_EXECUTE_ON_MATCHING_BUILTIN(OPERATOR_STRING)                                                          \
    if (left->output.type.if_vector != right->output.type.if_vector ||                                                 \
        left->output.type.if_matrix != right->output.type.if_matrix || left->output.type.if_struct ||                  \
        right->output.type.if_struct)                                                                                  \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATOR_STRING "\" on \"%s\" and \"%s\".", context->log_name,     \
                 resolve_scope->function->module_name, input_expression->source_name,                                  \
                 (long) input_expression->source_line,                                                                 \
                 get_type_name_for_logging (left->output.type.if_vector, left->output.type.if_matrix,                  \
                                            left->output.type.if_struct),                                              \
                 get_type_name_for_logging (right->output.type.if_vector, right->output.type.if_matrix,                \
                                            right->output.type.if_struct))                                             \
        return KAN_FALSE;                                                                                              \
    }

#define COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION                                                                    \
    result_expression->output.type.if_vector = left->output.type.if_vector;                                            \
    result_expression->output.type.if_matrix = left->output.type.if_matrix;                                            \
    result_expression->output.type.if_struct = left->output.type.if_struct;                                            \
    result_expression->output.boolean = left->output.boolean;                                                          \
    result_expression->output.writable = KAN_FALSE;                                                                    \
    result_expression->output.type.array_dimensions_count = 0u;                                                        \
    result_expression->output.type.array_dimensions = NULL

#define COPY_TYPE_FROM_RIGHT_FOR_ELEMENTAL_OPERATION                                                                   \
    result_expression->output.type.if_vector = right->output.type.if_vector;                                           \
    result_expression->output.type.if_matrix = right->output.type.if_matrix;                                           \
    result_expression->output.type.if_struct = right->output.type.if_struct;                                           \
    result_expression->output.boolean = right->output.boolean;                                                         \
    result_expression->output.writable = KAN_FALSE;                                                                    \
    result_expression->output.type.array_dimensions_count = 0u;                                                        \
    result_expression->output.type.array_dimensions = NULL

    case KAN_RPL_BINARY_OPERATION_ADD:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ADD;
        CANNOT_EXECUTE_ON_ARRAYS ("+")
        CAN_ONLY_EXECUTE_ON_MATCHING_BUILTIN ("+")
        COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_SUBTRACT:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_SUBTRACT;
        CANNOT_EXECUTE_ON_ARRAYS ("-")
        CAN_ONLY_EXECUTE_ON_MATCHING_BUILTIN ("-")
        COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_MULTIPLY:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_MULTIPLY;
        CANNOT_EXECUTE_ON_ARRAYS ("*")

        // Multiply vectors by elements.
        if (left->output.type.if_vector && left->output.type.if_vector == right->output.type.if_vector)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Multiply vector by scalar of the same type.
        if (left->output.type.if_vector && right->output.type.if_vector &&
            left->output.type.if_vector->item == right->output.type.if_vector->item &&
            right->output.type.if_vector->items_count == 1u)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Multiply matrix by scalar of the same type.
        if (left->output.type.if_matrix && right->output.type.if_vector &&
            left->output.type.if_matrix->item == right->output.type.if_vector->item &&
            right->output.type.if_vector->items_count == 1u)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Multiply matrix by vector of the same type.
        if (left->output.type.if_matrix && right->output.type.if_vector &&
            left->output.type.if_matrix->item == right->output.type.if_vector->item &&
            left->output.type.if_matrix->columns == right->output.type.if_vector->items_count)
        {
            COPY_TYPE_FROM_RIGHT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Multiply vector by matrix of the same type.
        if (left->output.type.if_vector && right->output.type.if_matrix &&
            left->output.type.if_vector->item == right->output.type.if_matrix->item &&
            left->output.type.if_vector->items_count == right->output.type.if_matrix->rows)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Multiply matrix by matrix of the same type.
        if (left->output.type.if_matrix && right->output.type.if_matrix &&
            left->output.type.if_matrix->item == right->output.type.if_matrix->item &&
            left->output.type.if_matrix->columns == right->output.type.if_matrix->rows)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Cannot execute \"*\" on \"%s\" and \"%s\".",
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                 (long) input_expression->source_line,
                 get_type_name_for_logging (left->output.type.if_vector, left->output.type.if_matrix,
                                            left->output.type.if_struct),
                 get_type_name_for_logging (right->output.type.if_vector, right->output.type.if_matrix,
                                            right->output.type.if_struct))
        return KAN_FALSE;

    case KAN_RPL_BINARY_OPERATION_DIVIDE:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_DIVIDE;
        CANNOT_EXECUTE_ON_ARRAYS ("/")

        // Divide vectors of the same type.
        if (left->output.type.if_vector && left->output.type.if_vector == right->output.type.if_vector)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Divide floating point matrices (integer point matrices are left out for simplicity).
        if (left->output.type.if_matrix && left->output.type.if_matrix == right->output.type.if_matrix)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Divide vector by scalar of the same type.
        if (left->output.type.if_vector && right->output.type.if_vector &&
            left->output.type.if_vector->item == right->output.type.if_vector->item &&
            right->output.type.if_vector->items_count == 1u)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Cannot execute \"/\" on \"%s\" and \"%s\".",
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                 (long) input_expression->source_line,
                 get_type_name_for_logging (left->output.type.if_vector, left->output.type.if_matrix,
                                            left->output.type.if_struct),
                 get_type_name_for_logging (right->output.type.if_vector, right->output.type.if_matrix,
                                            right->output.type.if_struct))
        return KAN_FALSE;

#define INTEGER_ONLY_VECTOR_OPERATION(OPERATION_STRING)                                                                \
    CANNOT_EXECUTE_ON_ARRAYS (OPERATION_STRING)                                                                        \
                                                                                                                       \
    if (!left->output.type.if_vector || !right->output.type.if_vector ||                                               \
        left->output.type.if_vector->item != INBUILT_TYPE_ITEM_INTEGER ||                                              \
        right->output.type.if_vector->item != INBUILT_TYPE_ITEM_INTEGER)                                               \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATION_STRING                                                   \
                 "\" on \"%s\" and \"%s\", only integer vectors are supported.",                                       \
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,               \
                 (long) input_expression->source_line,                                                                 \
                 get_type_name_for_logging (left->output.type.if_vector, left->output.type.if_matrix,                  \
                                            left->output.type.if_struct),                                              \
                 get_type_name_for_logging (right->output.type.if_vector, right->output.type.if_matrix,                \
                                            right->output.type.if_struct))                                             \
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

        if (!left->output.writable)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot execute \"=\" as its output is not writable.", context->log_name,
                     resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line)
            return KAN_FALSE;
        }

        if (left->output.type.if_vector != right->output.type.if_vector ||
            left->output.type.if_matrix != right->output.type.if_matrix ||
            left->output.type.if_struct != right->output.type.if_struct)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Cannot execute \"=\" on \"%s\" and \"%s\".",
                     context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line,
                     get_type_name_for_logging (left->output.type.if_vector, left->output.type.if_matrix,
                                                left->output.type.if_struct),
                     get_type_name_for_logging (right->output.type.if_vector, right->output.type.if_matrix,
                                                right->output.type.if_struct))
            return KAN_FALSE;
        }

        COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
        return KAN_TRUE;

#define LOGIC_OPERATION(OPERATION_STRING)                                                                              \
    CANNOT_EXECUTE_ON_ARRAYS (OPERATION_STRING)                                                                        \
    if (!left->output.boolean || !right->output.boolean)                                                               \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATION_STRING                                                   \
                 "\" on \"%s\" and \"%s\", only booleans are supported.",                                              \
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,               \
                 (long) input_expression->source_line,                                                                 \
                 get_type_name_for_logging (left->output.type.if_vector, left->output.type.if_matrix,                  \
                                            left->output.type.if_struct),                                              \
                 get_type_name_for_logging (right->output.type.if_vector, right->output.type.if_matrix,                \
                                            right->output.type.if_struct))                                             \
        return KAN_FALSE;                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    result_expression->output.type.if_vector = NULL;                                                                   \
    result_expression->output.type.if_matrix = NULL;                                                                   \
    result_expression->output.type.if_struct = NULL;                                                                   \
    result_expression->output.boolean = KAN_TRUE;                                                                      \
    result_expression->output.writable = KAN_FALSE;                                                                    \
    result_expression->output.type.array_dimensions_count = 0u;                                                        \
    result_expression->output.type.array_dimensions = NULL

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
                                                                                                                       \
    if (!left->output.type.if_vector || !right->output.type.if_vector ||                                               \
        left->output.type.if_vector->item != INBUILT_TYPE_ITEM_INTEGER ||                                              \
        right->output.type.if_vector->item != INBUILT_TYPE_ITEM_INTEGER)                                               \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATION_STRING                                                   \
                 "\" on \"%s\" and \"%s\", only integer vectors are supported.",                                       \
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,               \
                 (long) input_expression->source_line,                                                                 \
                 get_type_name_for_logging (left->output.type.if_vector, left->output.type.if_matrix,                  \
                                            left->output.type.if_struct),                                              \
                 get_type_name_for_logging (right->output.type.if_vector, right->output.type.if_matrix,                \
                                            right->output.type.if_struct))                                             \
    }                                                                                                                  \
                                                                                                                       \
    result_expression->output.boolean = KAN_TRUE

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
                                                                                                                       \
    if (!left->output.type.if_vector || !right->output.type.if_vector ||                                               \
        left->output.type.if_vector->items_count > 1u || right->output.type.if_vector->items_count > 1u ||             \
        left->output.type.if_vector->item != right->output.type.if_vector->item)                                       \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATION_STRING                                                   \
                 "\" on \"%s\" and \"%s\", only one-item vectors are supported.",                                      \
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,               \
                 (long) input_expression->source_line,                                                                 \
                 get_type_name_for_logging (left->output.type.if_vector, left->output.type.if_matrix,                  \
                                            left->output.type.if_struct),                                              \
                 get_type_name_for_logging (right->output.type.if_vector, right->output.type.if_matrix,                \
                                            right->output.type.if_struct))                                             \
    }                                                                                                                  \
                                                                                                                       \
    result_expression->output.boolean = KAN_TRUE

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
    if (operand->output.type.array_dimensions_count > 0u)
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Cannot execute unary operations on arrays.",
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                 (long) input_expression->source_line)
        return KAN_FALSE;
    }

    switch (input_expression->unary_operation.operation)
    {
    case KAN_RPL_UNARY_OPERATION_NEGATE:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NEGATE;
        if (!operand->output.type.if_vector && !operand->output.type.if_matrix)
        {
            KAN_LOG (
                rpl_compiler_context, KAN_LOG_ERROR,
                "[%s:%s:%s:%ld] Cannot apply \"~\" operation to type \"%s\", only vectors and matrices are supported.",
                context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                (long) input_expression->source_line,
                get_type_name_for_logging (operand->output.type.if_vector, operand->output.type.if_matrix,
                                           operand->output.type.if_struct))
            return KAN_FALSE;
        }

        result_expression->output.type.if_vector = operand->output.type.if_vector;
        result_expression->output.type.if_matrix = operand->output.type.if_matrix;
        return KAN_TRUE;

    case KAN_RPL_UNARY_OPERATION_NOT:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NOT;
        if (!operand->output.boolean)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot apply \"!\" operation to non-boolean type \"%s\".", context->log_name,
                     resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line,
                     get_type_name_for_logging (operand->output.type.if_vector, operand->output.type.if_matrix,
                                                operand->output.type.if_struct))
            return KAN_FALSE;
        }

        result_expression->output.boolean = KAN_TRUE;
        return KAN_TRUE;

    case KAN_RPL_UNARY_OPERATION_BITWISE_NOT:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_NOT;
        if (operand->output.type.if_vector != &STATICS.type_i1)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot apply \"~\" operation to type \"%s\", only i1 is supported.",
                     context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line,
                     get_type_name_for_logging (operand->output.type.if_vector, operand->output.type.if_matrix,
                                                operand->output.type.if_struct))
            return KAN_FALSE;
        }

        result_expression->output.type.if_vector = &STATICS.type_i1;
        return KAN_TRUE;
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

    new_expression->output.type.if_vector = NULL;
    new_expression->output.type.if_matrix = NULL;
    new_expression->output.type.if_struct = NULL;
    new_expression->output.type.array_dimensions_count = 0u;
    new_expression->output.type.array_dimensions = NULL;
    new_expression->output.boolean = KAN_FALSE;
    new_expression->output.writable = KAN_FALSE;

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
                if (buffer->first_flattened_declaration)
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Caught attempt to access flat buffer \"%s\" without selecting its field. "
                             "Flat buffers can not be accessed themselves as they're only containers for technically "
                             "separated data.",
                             context->log_name, resolve_scope->function->module_name, expression->source_name,
                             (long) expression->source_line, expression->identifier)
                    return KAN_FALSE;
                }

                new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE;
                new_expression->structured_buffer_reference = buffer;
                return resolve_use_buffer (context, instance, resolve_scope->function, expression->source_line,
                                           resolve_scope->function->required_stage, buffer);
            }

            buffer = buffer->next;
        }

        struct compiler_instance_scope_variable_item_t *variable =
            resolve_find_variable (resolve_scope, expression->identifier);

        if (variable)
        {
            new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_REFERENCE;
            new_expression->variable_reference = variable;

            new_expression->output.type.if_vector = variable->variable->type.if_vector;
            new_expression->output.type.if_matrix = variable->variable->type.if_matrix;
            new_expression->output.type.if_struct = variable->variable->type.if_struct;
            new_expression->output.type.array_dimensions_count = variable->variable->type.array_dimensions_count;
            new_expression->output.type.array_dimensions = variable->variable->type.array_dimensions;
            new_expression->output.boolean = KAN_FALSE;
            new_expression->output.writable = variable->writable;
            return KAN_TRUE;
        }

        // Search for option value that can be used here.
        for (uint64_t option_value_index = 0u; option_value_index < context->option_values.size; ++option_value_index)
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
                    new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_INTEGER_LITERAL;
                    new_expression->integer_literal = (int64_t) value->count_value;
                    new_expression->output.type.if_vector = &STATICS.type_i1;
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

    case KAN_RPL_EXPRESSION_NODE_TYPE_INTEGER_LITERAL:
        if (expression->integer_literal < INT32_MIN || expression->integer_literal > INT32_MAX)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Integer literal %lld is too big for some backends.", context->log_name,
                     resolve_scope->function->module_name, expression->source_name, (long) expression->source_line,
                     (long long) expression->integer_literal)
            return KAN_FALSE;
        }

        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_INTEGER_LITERAL;
        new_expression->integer_literal = expression->integer_literal;
        new_expression->output.type.if_vector = &STATICS.type_i1;
        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL:
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_FLOATING_LITERAL;
        new_expression->floating_literal = expression->floating_literal;
        new_expression->output.type.if_vector = &STATICS.type_f1;
        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_DECLARATION;
        kan_bool_t resolved = KAN_TRUE;

        if (!check_alias_or_variable_name_is_not_occupied (context, instance, resolve_scope,
                                                           expression->variable_declaration.variable_name))
        {
            resolved = KAN_FALSE;
        }

        new_expression->variable_declaration.variable.name = expression->variable_declaration.variable_name;
        new_expression->variable_declaration.variable.type.array_dimensions_count =
            expression->variable_declaration.array_size_expression_list_size;

        if (!resolve_variable_type (
                context, instance, new_expression->module_name, &new_expression->variable_declaration.variable,
                expression->variable_declaration.type_name, expression->variable_declaration.variable_name,
                expression->source_name, expression->source_line))
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_array_dimensions (context, instance, intermediate, &new_expression->variable_declaration.variable,
                                       expression->variable_declaration.array_size_expression_list_size,
                                       expression->variable_declaration.array_size_expression_list_index, KAN_TRUE))
        {
            resolved = KAN_FALSE;
        }

        new_expression->variable_declaration.declared_in_scope = NULL;
        if (resolved)
        {
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
                item->writable = KAN_TRUE;

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

            new_expression->output.type.if_vector = new_expression->variable_declaration.variable.type.if_vector;
            new_expression->output.type.if_matrix = new_expression->variable_declaration.variable.type.if_matrix;
            new_expression->output.type.if_struct = new_expression->variable_declaration.variable.type.if_struct;
            new_expression->output.type.array_dimensions_count =
                new_expression->variable_declaration.variable.type.array_dimensions_count;
            new_expression->output.type.array_dimensions =
                new_expression->variable_declaration.variable.type.array_dimensions;
            new_expression->output.boolean = KAN_FALSE;
            new_expression->output.writable = KAN_TRUE;
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

        for (uint64_t index = 0u; index < expression->scope.statement_list_size; ++index)
        {
            const uint64_t expression_index =
                ((uint64_t *)
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
        struct compiler_instance_sampler_node_t *sampler = instance->first_sampler;
        while (sampler)
        {
            if (sampler->name == expression->function_call.name)
            {
                new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_SAMPLER_CALL;
                new_expression->sampler_call.sampler = sampler;
                kan_bool_t resolved = KAN_TRUE;

                if (!resolve_use_sampler (context, instance, resolve_scope->function, sampler, expression->source_line))
                {
                    resolved = KAN_FALSE;
                }

                struct compiler_instance_declaration_node_t *signature_first_element = NULL;
                switch (sampler->type)
                {
                case KAN_RPL_SAMPLER_TYPE_2D:
                    signature_first_element = STATICS.sampler_2d_call_signature_first_element;
                    break;
                }

                if (!resolve_expression_array_with_signature (
                        context, instance, intermediate, resolve_scope, new_expression,
                        &new_expression->sampler_call.first_argument, expression->function_call.argument_list_size,
                        expression->function_call.argument_list_index, signature_first_element))
                {
                    resolved = KAN_FALSE;
                }

                new_expression->output.type.if_vector = &STATICS.type_f4;
                return resolved;
            }

            sampler = sampler->next;
        }

        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_FUNCTION_CALL;
        kan_bool_t resolved = KAN_TRUE;
        new_expression->function_call.function = NULL;
        new_expression->function_call.first_argument = NULL;

        if (!resolve_function_by_name (context, instance, expression->function_call.name,
                                       resolve_scope->function->required_stage,
                                       &new_expression->function_call.function))
        {
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
            new_expression->output.type.if_vector = new_expression->function_call.function->return_type_if_vector;
            new_expression->output.type.if_matrix = new_expression->function_call.function->return_type_if_matrix;
            new_expression->output.type.if_struct = new_expression->function_call.function->return_type_if_struct;
        }

        // We need to pass callee accesses to the caller function.
        if (resolved)
        {
            struct compiler_instance_buffer_access_node_t *buffer_access_node =
                new_expression->function_call.function->first_buffer_access;

            while (buffer_access_node)
            {
                struct compiler_instance_buffer_access_node_t *existent_access_node =
                    resolve_scope->function->first_buffer_access;

                while (existent_access_node)
                {
                    if (existent_access_node->buffer == buffer_access_node->buffer)
                    {
                        // Already used, no need for further verification.
                        break;
                    }

                    existent_access_node = existent_access_node->next;
                }

                if (!existent_access_node)
                {
                    struct compiler_instance_buffer_access_node_t *new_access_node =
                        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&instance->resolve_allocator,
                                                                  struct compiler_instance_buffer_access_node_t);

                    new_access_node->next = resolve_scope->function->first_buffer_access;
                    resolve_scope->function->first_buffer_access = new_access_node;
                    new_access_node->buffer = buffer_access_node->buffer;
                    new_access_node->direct_access_function = buffer_access_node->direct_access_function;
                    new_access_node->used_as_output = buffer_access_node->used_as_output;
                }

                buffer_access_node = buffer_access_node->next;
            }

            struct compiler_instance_sampler_access_node_t *sampler_access_node =
                new_expression->function_call.function->first_sampler_access;

            while (sampler_access_node)
            {
                struct compiler_instance_sampler_access_node_t *existent_access_node =
                    resolve_scope->function->first_sampler_access;

                while (existent_access_node)
                {
                    if (existent_access_node->sampler == sampler_access_node->sampler)
                    {
                        // Already used, no need fop further verification.
                        return KAN_TRUE;
                    }

                    existent_access_node = existent_access_node->next;
                }

                if (!existent_access_node)
                {
                    struct compiler_instance_sampler_access_node_t *new_access_node =
                        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&instance->resolve_allocator,
                                                                  struct compiler_instance_sampler_access_node_t);

                    new_access_node->next = resolve_scope->function->first_sampler_access;
                    resolve_scope->function->first_sampler_access = new_access_node;
                    new_access_node->sampler = sampler_access_node->sampler;
                    new_access_node->direct_access_function = sampler_access_node->direct_access_function;
                }

                sampler_access_node = sampler_access_node->next;
            }

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
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_CONSTRUCTOR;
        kan_bool_t resolved = KAN_TRUE;
        new_expression->constructor.type_if_vector = NULL;
        new_expression->constructor.type_if_matrix = NULL;
        new_expression->constructor.type_if_struct = NULL;

        if (!(new_expression->constructor.type_if_vector =
                  find_inbuilt_vector_type (expression->constructor.type_name)) &&
            !(new_expression->constructor.type_if_matrix =
                  find_inbuilt_matrix_type (expression->constructor.type_name)) &&
            !resolve_use_struct (context, instance, expression->constructor.type_name,
                                 &new_expression->constructor.type_if_struct))
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Constructor \"%s\" type is unknown.",
                     context->log_name, new_expression->module_name, new_expression->source_name,
                     (long) new_expression->source_line, expression->constructor.type_name)
            resolved = KAN_FALSE;
        }

        if (resolved)
        {
            struct compiler_instance_declaration_node_t *signature = NULL;
            if (new_expression->constructor.type_if_vector)
            {
                signature = new_expression->constructor.type_if_vector->constructor_signature;
            }
            else if (new_expression->constructor.type_if_matrix)
            {
                signature = new_expression->constructor.type_if_matrix->constructor_signature;
            }
            else if (new_expression->constructor.type_if_struct)
            {
                signature = new_expression->constructor.type_if_struct->first_field;
            }

            if (!resolve_expression_array_with_signature (context, instance, intermediate, resolve_scope,
                                                          new_expression, &new_expression->constructor.first_argument,
                                                          expression->constructor.argument_list_size,
                                                          expression->constructor.argument_list_index, signature))
            {
                resolved = KAN_FALSE;
            }
        }

        new_expression->output.type.if_vector = new_expression->constructor.type_if_vector;
        new_expression->output.type.if_matrix = new_expression->constructor.type_if_matrix;
        new_expression->output.type.if_struct = new_expression->constructor.type_if_struct;
        return resolved;
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
            if (!new_expression->if_.condition->output.boolean)
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
            if (!loop_expression->for_.condition->output.boolean)
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
            if (!new_expression->while_.condition->output.boolean)
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
                if (!resolve_scope->function->return_type_if_vector &&
                    !resolve_scope->function->return_type_if_matrix && !resolve_scope->function->return_type_if_struct)
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Caught attempt to return \"%s\" from function \"%s\" which returns void.",
                             context->log_name, new_expression->module_name, new_expression->source_name,
                             (long) new_expression->source_line,
                             get_type_name_for_logging (new_expression->return_expression->output.type.if_vector,
                                                        new_expression->return_expression->output.type.if_matrix,
                                                        new_expression->return_expression->output.type.if_struct),
                             resolve_scope->function->name)
                    resolved = KAN_FALSE;
                }

                if (new_expression->return_expression->output.type.array_dimensions_count > 0u)
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Caught return of array from function \"%s\" which is not supported.",
                             context->log_name, new_expression->module_name, new_expression->source_name,
                             (long) new_expression->source_line, resolve_scope->function->name)
                    resolved = KAN_FALSE;
                }

                if (new_expression->return_expression->output.boolean)
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Caught return of boolean from function \"%s\" which is not supported as "
                             "independent type.",
                             context->log_name, new_expression->module_name, new_expression->source_name,
                             (long) new_expression->source_line, resolve_scope->function->name)
                    resolved = KAN_FALSE;
                }

                if ((resolve_scope->function->return_type_if_vector &&
                     new_expression->return_expression->output.type.if_vector !=
                         resolve_scope->function->return_type_if_vector) ||
                    (resolve_scope->function->return_type_if_matrix &&
                     new_expression->return_expression->output.type.if_matrix !=
                         resolve_scope->function->return_type_if_matrix) ||
                    (resolve_scope->function->return_type_if_struct &&
                     new_expression->return_expression->output.type.if_struct !=
                         resolve_scope->function->return_type_if_struct))
                {
                    KAN_LOG (
                        rpl_compiler_context, KAN_LOG_ERROR,
                        "[%s:%s:%s:%ld] Caught attempt to return \"%s\" from function \"%s\" which returns \"%s\".",
                        context->log_name, new_expression->module_name, new_expression->source_name,
                        (long) new_expression->source_line,
                        get_type_name_for_logging (new_expression->return_expression->output.type.if_vector,
                                                   new_expression->return_expression->output.type.if_matrix,
                                                   new_expression->return_expression->output.type.if_struct),
                        resolve_scope->function->name,
                        get_type_name_for_logging (resolve_scope->function->return_type_if_vector,
                                                   resolve_scope->function->return_type_if_matrix,
                                                   resolve_scope->function->return_type_if_struct))
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
            if (resolve_scope->function->return_type_if_vector || resolve_scope->function->return_type_if_matrix ||
                resolve_scope->function->return_type_if_struct)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Caught void return from function \"%s\" which returns \"%s\".",
                         context->log_name, new_expression->module_name, new_expression->source_name,
                         (long) new_expression->source_line, resolve_scope->function->name,
                         get_type_name_for_logging (resolve_scope->function->return_type_if_vector,
                                                    resolve_scope->function->return_type_if_matrix,
                                                    resolve_scope->function->return_type_if_struct))
                resolved = KAN_FALSE;
            }
        }

        return resolved;
    }
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
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

    function_node->return_type_if_vector = NULL;
    function_node->return_type_if_matrix = NULL;
    function_node->return_type_if_struct = NULL;

    if (function->return_type_name != STATICS.interned_void)
    {
        if (!(function_node->return_type_if_vector = find_inbuilt_vector_type (function->return_type_name)) &&
            !(function_node->return_type_if_matrix = find_inbuilt_matrix_type (function->return_type_name)) &&
            !resolve_use_struct (context, instance, function->return_type_name, &function_node->return_type_if_struct))
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Function return type \"%s\" is unknown.",
                     context->log_name, intermediate->log_name, function->source_name, (long) function->source_line,
                     function->return_type_name)
            resolved = KAN_FALSE;
        }
    }

    function_node->has_stage_specific_access = KAN_FALSE;
    function_node->required_stage = context_stage;
    function_node->first_buffer_access = NULL;
    function_node->first_sampler_access = NULL;

    function_node->module_name = intermediate->log_name;
    function_node->source_name = function->source_name;
    function_node->source_line = function->source_line;

    if (!resolve_declarations (context, instance, intermediate, &function->arguments, &function_node->first_argument,
                               KAN_TRUE))
    {
        resolved = KAN_FALSE;
    }

    struct compiler_instance_declaration_node_t *argument_declaration = function_node->first_argument;
    function_node->first_argument_variable = NULL;
    struct compiler_instance_scope_variable_item_t *last_argument_variable = NULL;

    while (argument_declaration)
    {
        struct compiler_instance_scope_variable_item_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &instance->resolve_allocator, struct compiler_instance_scope_variable_item_t);

        item->next = NULL;
        item->variable = &argument_declaration->variable;
        item->writable = KAN_FALSE;

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

    if (function_node->return_type_if_vector || function_node->return_type_if_matrix ||
        function_node->return_type_if_struct)
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

        struct compiler_instance_buffer_access_node_t *buffer_access = function_node->first_buffer_access;
        while (buffer_access)
        {
            switch (buffer_access->buffer->type)
            {
            case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
            case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
            case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s] Function \"%s\" accesses stage-specific buffer \"%s\" through call of "
                         "function \"%s\".",
                         context->log_name, function_node->module_name, function_node->name,
                         buffer_access->buffer->name, buffer_access->direct_access_function->name)
                break;

            case KAN_RPL_BUFFER_TYPE_UNIFORM:
            case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
                break;
            }

            buffer_access = buffer_access->next;
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

    for (uint64_t intermediate_index = 0u; intermediate_index < context->modules.size; ++intermediate_index)
    {
        struct kan_rpl_intermediate_t *intermediate =
            ((struct kan_rpl_intermediate_t **) context->modules.data)[intermediate_index];

        for (uint64_t function_index = 0u; function_index < intermediate->functions.size; ++function_index)
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
                                                              uint64_t entry_point_count,
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

    instance->first_buffer = NULL;
    instance->last_buffer = NULL;

    instance->first_sampler = NULL;
    instance->last_sampler = NULL;

    instance->first_function = NULL;
    instance->last_function = NULL;

    kan_bool_t successfully_resolved = KAN_TRUE;
    struct binding_location_assignment_counter_t assignment_counter = {
        .next_attribute_buffer_binding = 0u,
        .next_arbitrary_stable_buffer_binding = 0u,
        .next_arbitrary_unstable_buffer_binding = 0u,
        .next_attribute_location = 0u,
        .next_vertex_output_location = 0u,
        .next_fragment_output_location = 0u,
    };

    for (uint64_t intermediate_index = 0u; intermediate_index < context->modules.size; ++intermediate_index)
    {
        struct kan_rpl_intermediate_t *intermediate =
            ((struct kan_rpl_intermediate_t **) context->modules.data)[intermediate_index];

        if (!resolve_settings (context, instance, intermediate, &intermediate->settings, KAN_TRUE,
                               &instance->first_setting, &instance->last_setting))
        {
            successfully_resolved = KAN_FALSE;
        }

        // Buffers and samplers are always added even if they're not used to preserve shader family compatibility.

        if (!resolve_buffers (context, instance, intermediate, &assignment_counter))
        {
            successfully_resolved = KAN_FALSE;
        }

        if (!resolve_samplers (context, instance, intermediate, &assignment_counter))
        {
            successfully_resolved = KAN_FALSE;
        }
    }

    for (uint64_t entry_point_index = 0u; entry_point_index < entry_point_count; ++entry_point_index)
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
