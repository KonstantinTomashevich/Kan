#include <kan/c_interface/builder.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>

KAN_LOG_DEFINE_CATEGORY (c_interface_builder);

static kan_bool_t count_and_allocate_globals (struct kan_c_interface_t *interface,
                                              const struct kan_c_token_t *token_sequence)
{
    const struct kan_c_token_t *token = token_sequence;
    while (token)
    {
        switch (token->type)
        {
        case KAN_C_TOKEN_MARKER_META:
        case KAN_C_TOKEN_INTEGER_META:
        case KAN_C_TOKEN_STRING_META:
        case KAN_C_TOKEN_ENUM_VALUE:
        case KAN_C_TOKEN_ENUM_END:
        case KAN_C_TOKEN_STRUCT_FIELD:
        case KAN_C_TOKEN_STRUCT_END:
        case KAN_C_TOKEN_FUNCTION_ARGUMENT:
        case KAN_C_TOKEN_FUNCTION_END:
            break;

        case KAN_C_TOKEN_ENUM_BEGIN:
            ++interface->enums_count;
            break;

        case KAN_C_TOKEN_STRUCT_BEGIN:
            ++interface->structs_count;
            break;

        case KAN_C_TOKEN_FUNCTION_BEGIN:
            ++interface->functions_count;
            break;

        case KAN_C_TOKEN_SYMBOL:
            ++interface->symbols_count;
            break;
        }

        token = token->next;
    }

    kan_c_interface_init_enums_array (interface, interface->enums_count);
    kan_c_interface_init_structs_array (interface, interface->structs_count);
    kan_c_interface_init_functions_array (interface, interface->functions_count);
    kan_c_interface_init_symbols_array (interface, interface->symbols_count);
    return KAN_TRUE;
}

static kan_bool_t add_meta (struct kan_c_meta_attachment_t *attachment,
                            const struct kan_c_token_t *first_meta,
                            uint64_t meta_count)
{
    attachment->meta_count = meta_count;
    if (attachment->meta_count == 0u)
    {
        attachment->meta_array = NULL;
    }
    else
    {
        attachment->meta_array = kan_allocate_general (kan_c_interface_allocation_group (),
                                                       sizeof (struct kan_c_meta_t) * attachment->meta_count,
                                                       _Alignof (struct kan_c_meta_t));
        struct kan_c_meta_t *meta = attachment->meta_array;

        while (meta_count > 0u)
        {
            if (!first_meta)
            {
                KAN_LOG (c_interface_builder, KAN_LOG_ERROR, "Encountered end of tokens while adding meta.")
                return KAN_FALSE;
            }

            switch (first_meta->type)
            {
            case KAN_C_TOKEN_MARKER_META:
                meta->type = KAN_C_META_MARKER;
                meta->name = first_meta->marker_meta.name;
                break;

            case KAN_C_TOKEN_INTEGER_META:
                meta->type = KAN_C_META_INTEGER;
                meta->name = first_meta->integer_meta.name;
                meta->integer_value = first_meta->integer_meta.value;
                break;

            case KAN_C_TOKEN_STRING_META:
                meta->type = KAN_C_META_STRING;
                meta->name = first_meta->string_meta.name;
                meta->string_value = first_meta->string_meta.value;
                break;

            case KAN_C_TOKEN_ENUM_VALUE:
            case KAN_C_TOKEN_ENUM_BEGIN:
            case KAN_C_TOKEN_ENUM_END:
            case KAN_C_TOKEN_STRUCT_FIELD:
            case KAN_C_TOKEN_STRUCT_BEGIN:
            case KAN_C_TOKEN_STRUCT_END:
            case KAN_C_TOKEN_FUNCTION_ARGUMENT:
            case KAN_C_TOKEN_FUNCTION_BEGIN:
            case KAN_C_TOKEN_FUNCTION_END:
            case KAN_C_TOKEN_SYMBOL:
                KAN_LOG (c_interface_builder, KAN_LOG_ERROR, "Encountered unexpected token while adding meta: %d.",
                         first_meta->type)
                return KAN_FALSE;
            }

            ++meta;
            first_meta = first_meta->next;
            --meta_count;
        }
    }

    return KAN_TRUE;
}

#define CASE_META                                                                                                      \
    case KAN_C_TOKEN_MARKER_META:                                                                                      \
    case KAN_C_TOKEN_INTEGER_META:                                                                                     \
    case KAN_C_TOKEN_STRING_META:                                                                                      \
        if (first_meta == NULL)                                                                                        \
        {                                                                                                              \
            first_meta = token;                                                                                        \
            meta_count = 1u;                                                                                           \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            ++meta_count;                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        break

#define CLEAR_META                                                                                                     \
    first_meta = NULL;                                                                                                 \
    meta_count = 0u

static kan_bool_t add_enum (struct kan_c_enum_t *enum_info,
                            const struct kan_c_token_t **token_sequence,
                            const struct kan_c_token_t *first_meta,
                            uint64_t meta_count)
{
    const struct kan_c_token_t *token = *token_sequence;
    uint64_t values_count = 0u;

    while (token && token->type != KAN_C_TOKEN_ENUM_END)
    {
        if (token->type == KAN_C_TOKEN_ENUM_VALUE)
        {
            ++values_count;
        }

        token = token->next;
    }

    enum_info->name = (*token_sequence)->enum_begin.name;
    kan_c_enum_init_values_array (enum_info, values_count);
    add_meta (&enum_info->meta, first_meta, meta_count);
    struct kan_c_enum_value_t *output_enum_value = enum_info->values;

    CLEAR_META;
    token = (*token_sequence)->next;

    while (token && token->type != KAN_C_TOKEN_ENUM_END)
    {
        switch (token->type)
        {
            CASE_META;

        case KAN_C_TOKEN_ENUM_VALUE:
            output_enum_value->name = token->enum_value.name;
            if (!add_meta (&output_enum_value->meta, first_meta, meta_count))
            {
                return KAN_FALSE;
            }

            ++output_enum_value;
            CLEAR_META;
            break;

        case KAN_C_TOKEN_ENUM_BEGIN:
        case KAN_C_TOKEN_STRUCT_FIELD:
        case KAN_C_TOKEN_STRUCT_BEGIN:
        case KAN_C_TOKEN_STRUCT_END:
        case KAN_C_TOKEN_FUNCTION_ARGUMENT:
        case KAN_C_TOKEN_FUNCTION_BEGIN:
        case KAN_C_TOKEN_FUNCTION_END:
        case KAN_C_TOKEN_SYMBOL:
            KAN_LOG (c_interface_builder, KAN_LOG_ERROR, "Encountered unexpected token while adding enum values: %d.",
                     token->type)
            return KAN_FALSE;

        case KAN_C_TOKEN_ENUM_END:
            KAN_ASSERT (KAN_FALSE)
            break;
        }

        token = token->next;
    }

    if (!token)
    {
        KAN_LOG (c_interface_builder, KAN_LOG_ERROR, "Encountered end of tokens while adding enum.")
        return KAN_FALSE;
    }

    *token_sequence = token;
    return KAN_TRUE;
}

static kan_bool_t add_struct (struct kan_c_struct_t *struct_info,
                              const struct kan_c_token_t **token_sequence,
                              const struct kan_c_token_t *first_meta,
                              uint64_t meta_count)
{
    const struct kan_c_token_t *token = *token_sequence;
    uint64_t fields_count = 0u;

    while (token && token->type != KAN_C_TOKEN_STRUCT_END)
    {
        if (token->type == KAN_C_TOKEN_STRUCT_FIELD)
        {
            ++fields_count;
        }

        token = token->next;
    }

    struct_info->name = (*token_sequence)->struct_begin.name;
    kan_c_struct_init_fields_array (struct_info, fields_count);
    add_meta (&struct_info->meta, first_meta, meta_count);
    struct kan_c_variable_t *output_field_value = struct_info->fields;

    CLEAR_META;
    token = (*token_sequence)->next;

    while (token && token->type != KAN_C_TOKEN_STRUCT_END)
    {
        switch (token->type)
        {
            CASE_META;

        case KAN_C_TOKEN_STRUCT_FIELD:
            output_field_value->name = token->struct_field.name;
            output_field_value->type = token->struct_field.type;

            if (!add_meta (&output_field_value->meta, first_meta, meta_count))
            {
                return KAN_FALSE;
            }

            ++output_field_value;
            CLEAR_META;
            break;

        case KAN_C_TOKEN_ENUM_VALUE:
        case KAN_C_TOKEN_ENUM_END:
        case KAN_C_TOKEN_ENUM_BEGIN:
        case KAN_C_TOKEN_STRUCT_BEGIN:
        case KAN_C_TOKEN_FUNCTION_ARGUMENT:
        case KAN_C_TOKEN_FUNCTION_BEGIN:
        case KAN_C_TOKEN_FUNCTION_END:
        case KAN_C_TOKEN_SYMBOL:
            KAN_LOG (c_interface_builder, KAN_LOG_ERROR, "Encountered unexpected token while adding struct fields: %d.",
                     token->type)
            return KAN_FALSE;

        case KAN_C_TOKEN_STRUCT_END:
            KAN_ASSERT (KAN_FALSE)
            break;
        }

        token = token->next;
    }

    if (!token)
    {
        KAN_LOG (c_interface_builder, KAN_LOG_ERROR, "Encountered end of tokens while adding struct.")
        return KAN_FALSE;
    }

    *token_sequence = token;
    return KAN_TRUE;
}

static kan_bool_t add_function (struct kan_c_function_t *function_info,
                                const struct kan_c_token_t **token_sequence,
                                const struct kan_c_token_t *first_meta,
                                uint64_t meta_count)
{
    const struct kan_c_token_t *token = *token_sequence;
    uint64_t arguments_count = 0u;

    while (token && token->type != KAN_C_TOKEN_FUNCTION_END)
    {
        if (token->type == KAN_C_TOKEN_FUNCTION_ARGUMENT)
        {
            ++arguments_count;
        }

        token = token->next;
    }

    function_info->name = (*token_sequence)->function_begin.name;
    function_info->return_type = (*token_sequence)->function_begin.return_type;
    kan_c_function_init_arguments_array (function_info, arguments_count);
    add_meta (&function_info->meta, first_meta, meta_count);
    struct kan_c_variable_t *output_argument_value = function_info->arguments;

    CLEAR_META;
    token = (*token_sequence)->next;

    while (token && token->type != KAN_C_TOKEN_FUNCTION_END)
    {
        switch (token->type)
        {
            CASE_META;

        case KAN_C_TOKEN_FUNCTION_ARGUMENT:
            output_argument_value->name = token->function_argument.name;
            output_argument_value->type = token->function_argument.type;

            if (!add_meta (&output_argument_value->meta, first_meta, meta_count))
            {
                return KAN_FALSE;
            }

            ++output_argument_value;
            CLEAR_META;
            break;

        case KAN_C_TOKEN_ENUM_VALUE:
        case KAN_C_TOKEN_ENUM_END:
        case KAN_C_TOKEN_ENUM_BEGIN:
        case KAN_C_TOKEN_STRUCT_FIELD:
        case KAN_C_TOKEN_STRUCT_BEGIN:
        case KAN_C_TOKEN_STRUCT_END:
        case KAN_C_TOKEN_FUNCTION_BEGIN:
        case KAN_C_TOKEN_SYMBOL:
            KAN_LOG (c_interface_builder, KAN_LOG_ERROR,
                     "Encountered unexpected token while adding function arguments: %d.", token->type)
            return KAN_FALSE;

        case KAN_C_TOKEN_FUNCTION_END:
            KAN_ASSERT (KAN_FALSE)
            break;
        }

        token = token->next;
    }

    if (!token)
    {
        KAN_LOG (c_interface_builder, KAN_LOG_ERROR, "Encountered end of tokens while adding function.")
        return KAN_FALSE;
    }

    *token_sequence = token;
    return KAN_TRUE;
}

static kan_bool_t parse (struct kan_c_interface_t *interface, const struct kan_c_token_t *token_sequence)
{
    const struct kan_c_token_t *first_meta = NULL;
    uint64_t meta_count = 0u;
    const struct kan_c_token_t *token = token_sequence;

    struct kan_c_enum_t *output_enum = interface->enums;
    struct kan_c_struct_t *output_struct = interface->structs;
    struct kan_c_function_t *output_function = interface->functions;
    struct kan_c_variable_t *output_symbol = interface->symbols;

    while (token)
    {
        switch (token->type)
        {
            CASE_META;

        case KAN_C_TOKEN_ENUM_VALUE:
        case KAN_C_TOKEN_ENUM_END:
        case KAN_C_TOKEN_STRUCT_FIELD:
        case KAN_C_TOKEN_STRUCT_END:
        case KAN_C_TOKEN_FUNCTION_ARGUMENT:
        case KAN_C_TOKEN_FUNCTION_END:
            KAN_LOG (c_interface_builder, KAN_LOG_ERROR, "Unexpected token in global scope: %d.", token->type)
            return KAN_FALSE;

        case KAN_C_TOKEN_ENUM_BEGIN:
            if (!add_enum (output_enum, &token, first_meta, meta_count))
            {
                return KAN_FALSE;
            }

            ++output_enum;
            CLEAR_META;
            break;

        case KAN_C_TOKEN_STRUCT_BEGIN:
            if (!add_struct (output_struct, &token, first_meta, meta_count))
            {
                return KAN_FALSE;
            }

            ++output_struct;
            CLEAR_META;
            break;

        case KAN_C_TOKEN_FUNCTION_BEGIN:
            if (!add_function (output_function, &token, first_meta, meta_count))
            {
                return KAN_FALSE;
            }

            ++output_function;
            CLEAR_META;
            break;

        case KAN_C_TOKEN_SYMBOL:
            output_symbol->name = token->symbol.name;
            output_symbol->type = token->symbol.type;

            if (!add_meta (&output_symbol->meta, first_meta, meta_count))
            {
                return KAN_FALSE;
            }

            ++output_symbol;
            CLEAR_META;
            break;
        }

        token = token->next;
    }

    return KAN_TRUE;
}

struct kan_c_interface_t *kan_c_interface_build (const struct kan_c_token_t *token_sequence)
{
    struct kan_c_interface_t *interface = (struct kan_c_interface_t *) kan_allocate_batched (
        kan_c_interface_allocation_group (), sizeof (struct kan_c_interface_t));

    interface->enums_count = 0u;
    interface->enums = NULL;

    interface->structs_count = 0u;
    interface->structs = NULL;

    interface->functions_count = 0u;
    interface->functions = NULL;

    interface->symbols_count = 0u;
    interface->symbols = NULL;

    if (count_and_allocate_globals (interface, token_sequence) && parse (interface, token_sequence))
    {
        return interface;
    }

    kan_c_interface_destroy (interface);
    return NULL;
}

void kan_c_interface_init_enums_array (struct kan_c_interface_t *interface, uint64_t count)
{
    interface->enums_count = count;
    if (interface->enums_count == 0u)
    {
        interface->enums = NULL;
    }
    else
    {
        interface->enums = kan_allocate_general (kan_c_interface_allocation_group (),
                                                 sizeof (struct kan_c_enum_t) * interface->enums_count,
                                                 _Alignof (struct kan_c_enum_t));

        for (uint64_t index = 0u; index < interface->enums_count; ++index)
        {
            struct kan_c_enum_t *enum_info = &interface->enums[index];
            enum_info->values_count = 0u;
            enum_info->values = NULL;
            enum_info->meta.meta_count = 0u;
            enum_info->meta.meta_array = NULL;
        }
    }
}

void kan_c_interface_init_structs_array (struct kan_c_interface_t *interface, uint64_t count)
{
    interface->structs_count = count;
    if (interface->structs_count == 0u)
    {
        interface->structs = NULL;
    }
    else
    {
        interface->structs = kan_allocate_general (kan_c_interface_allocation_group (),
                                                   sizeof (struct kan_c_struct_t) * interface->structs_count,
                                                   _Alignof (struct kan_c_struct_t));

        for (uint64_t index = 0u; index < interface->structs_count; ++index)
        {
            struct kan_c_struct_t *struct_info = &interface->structs[index];
            struct_info->fields_count = 0u;
            struct_info->fields = NULL;
            struct_info->meta.meta_count = 0u;
            struct_info->meta.meta_array = NULL;
        }
    }
}

void kan_c_interface_init_functions_array (struct kan_c_interface_t *interface, uint64_t count)
{
    interface->functions_count = count;
    if (interface->functions_count == 0u)
    {
        interface->functions = NULL;
    }
    else
    {
        interface->functions = kan_allocate_general (kan_c_interface_allocation_group (),
                                                     sizeof (struct kan_c_function_t) * interface->functions_count,
                                                     _Alignof (struct kan_c_function_t));

        for (uint64_t index = 0u; index < interface->functions_count; ++index)
        {
            struct kan_c_function_t *function_info = &interface->functions[index];
            function_info->arguments_count = 0u;
            function_info->arguments = NULL;
            function_info->meta.meta_count = 0u;
            function_info->meta.meta_array = NULL;
        }
    }
}

void kan_c_interface_init_symbols_array (struct kan_c_interface_t *interface, uint64_t count)
{
    interface->symbols_count = count;
    if (interface->symbols_count == 0u)
    {
        interface->symbols = NULL;
    }
    else
    {
        interface->symbols = kan_allocate_general (kan_c_interface_allocation_group (),
                                                   sizeof (struct kan_c_variable_t) * interface->symbols_count,
                                                   _Alignof (struct kan_c_variable_t));

        for (uint64_t index = 0u; index < interface->symbols_count; ++index)
        {
            struct kan_c_variable_t *symbol_info = &interface->symbols[index];
            symbol_info->meta.meta_count = 0u;
            symbol_info->meta.meta_array = NULL;
        }
    }
}

void kan_c_enum_init_values_array (struct kan_c_enum_t *enum_info, uint64_t count)
{
    enum_info->values_count = count;
    if (enum_info->values_count == 0u)
    {
        enum_info->values = NULL;
    }
    else
    {
        enum_info->values = kan_allocate_general (kan_c_interface_allocation_group (),
                                                  sizeof (struct kan_c_enum_value_t) * enum_info->values_count,
                                                  _Alignof (struct kan_c_enum_value_t));

        for (uint64_t value_index = 0u; value_index < enum_info->values_count; ++value_index)
        {
            struct kan_c_enum_value_t *value_info = &enum_info->values[value_index];
            value_info->meta.meta_count = 0u;
            value_info->meta.meta_array = NULL;
        }
    }
}

void kan_c_struct_init_fields_array (struct kan_c_struct_t *struct_info, uint64_t count)
{
    struct_info->fields_count = count;
    if (struct_info->fields_count == 0u)
    {
        struct_info->fields = NULL;
    }
    {
        struct_info->fields = kan_allocate_general (kan_c_interface_allocation_group (),
                                                    sizeof (struct kan_c_variable_t) * struct_info->fields_count,
                                                    _Alignof (struct kan_c_variable_t));

        for (uint64_t index = 0u; index < struct_info->fields_count; ++index)
        {
            struct kan_c_variable_t *info = &struct_info->fields[index];
            info->meta.meta_count = 0u;
            info->meta.meta_array = NULL;
        }
    }
}

void kan_c_function_init_arguments_array (struct kan_c_function_t *function_info, uint64_t count)
{
    function_info->arguments_count = count;
    if (function_info->arguments_count == 0u)
    {
        function_info->arguments = NULL;
    }
    else
    {
        function_info->arguments = kan_allocate_general (
            kan_c_interface_allocation_group (), sizeof (struct kan_c_variable_t) * function_info->arguments_count,
            _Alignof (struct kan_c_variable_t));

        // Clean fields that will be needed for correct destruction deserialization error.
        for (uint64_t index = 0u; index < function_info->arguments_count; ++index)
        {
            struct kan_c_variable_t *info = &function_info->arguments[index];
            info->meta.meta_count = 0u;
            info->meta.meta_array = NULL;
        }
    }
}
