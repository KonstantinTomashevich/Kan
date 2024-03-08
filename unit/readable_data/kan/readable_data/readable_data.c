#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>

#include <kan/api_common/mute_third_party_warnings.h>
#include <kan/container/list.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/readable_data/readable_data.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (readable_data);

struct re2c_tags_t
{
    /*!stags:re2c format = 'const char *@@;';*/
};

struct parser_t
{
    struct kan_stream_t *stream;
    struct kan_stack_group_allocator_t temporary_allocator;
    struct kan_readable_data_event_t current_event;

    char *limit;
    const char *cursor;
    const char *marker;
    const char *token;

    kan_bool_t end_of_input_reached;
    size_t cursor_line;
    size_t cursor_symbol;
    size_t marker_line;
    size_t marker_symbol;

    const char *saved;
    size_t saved_line;
    size_t saved_symbol;

    size_t opened_blocks;
    struct re2c_tags_t tags;

    char input_buffer[KAN_READABLE_DATA_PARSE_INPUT_BUFFER_SIZE];
};

struct emitter_t
{
    struct kan_stream_t *stream;
    uint64_t indentation_level;
    char formatting_buffer[KAN_READABLE_DATA_EMIT_FORMATTING_BUFFER_SIZE];
};

static kan_allocation_group_t readable_data_allocation_group;
static kan_allocation_group_t readable_data_temporary_allocation_group;

static kan_bool_t statics_initialized = KAN_FALSE;
static struct kan_atomic_int_t statics_initialization_lock = {.value = 0};

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        kan_atomic_int_lock (&statics_initialization_lock);
        if (!statics_initialized)
        {
            readable_data_allocation_group =
                kan_allocation_group_get_child (kan_allocation_group_root (), "readable_data");
            readable_data_temporary_allocation_group =
                kan_allocation_group_get_child (readable_data_allocation_group, "temporary");
            statics_initialized = KAN_TRUE;
        }

        kan_atomic_int_unlock (&statics_initialization_lock);
    }
}

static int re2c_refill_buffer (struct parser_t *parser)
{
#define ERROR_REFILL_AFTER_END_OF_FILE -1
#define ERROR_LEXEME_OVERFLOW -2

    if (parser->end_of_input_reached)
    {
        return ERROR_REFILL_AFTER_END_OF_FILE;
    }

    const size_t shift = parser->token - parser->input_buffer;
    const size_t used = parser->limit - parser->token;

    if (shift < 1)
    {
        return ERROR_LEXEME_OVERFLOW;
    }

    // Shift buffer contents (discard everything up to the current token).
    memmove (parser->input_buffer, parser->token, used);
    parser->limit -= shift;
    parser->cursor -= shift;
    parser->marker -= shift;
    parser->token -= shift;

    const char **first_tag = (const char **) &parser->tags;
    const char **last_tag = first_tag + sizeof (struct re2c_tags_t) / sizeof (char *);

    while (first_tag != last_tag)
    {
        if (*first_tag)
        {
            *first_tag -= shift;
        }

        ++first_tag;
    }

    // Fill free space at the end of buffer with new data from file.
    parser->limit += parser->stream->operations->read (
        parser->stream, KAN_READABLE_DATA_PARSE_INPUT_BUFFER_SIZE - used - 1u, parser->limit);
    parser->limit[0u] = 0;
    parser->end_of_input_reached =
        parser->limit < parser->input_buffer + KAN_READABLE_DATA_PARSE_INPUT_BUFFER_SIZE - 1u;

    return 0;

#undef ERROR_REFILL_AFTER_END_OF_FILE
#undef ERROR_LEXEME_OVERFLOW
}

static inline void re2c_yyskip (struct parser_t *parser)
{
    if (*parser->cursor == '\n')
    {
        ++parser->cursor_line;
        parser->cursor_symbol = 0u;
    }

    ++parser->cursor;
    ++parser->cursor_symbol;
}

static inline void re2c_yybackup (struct parser_t *parser)
{
    parser->marker = parser->cursor;
    parser->marker_line = parser->cursor_line;
    parser->marker_symbol = parser->cursor_symbol;
}

static inline void re2c_yyrestore (struct parser_t *parser)
{
    parser->cursor = parser->marker;
    parser->cursor_line = parser->marker_line;
    parser->cursor_symbol = parser->marker_symbol;
}

static inline void re2c_save_cursor (struct parser_t *parser)
{
    parser->saved = parser->cursor;
    parser->saved_line = parser->cursor_line;
    parser->saved_symbol = parser->cursor_symbol;
}

static inline void re2c_restore_saved_cursor (struct parser_t *parser)
{
    parser->cursor = parser->saved;
    parser->cursor_line = parser->saved_line;
    parser->cursor_symbol = parser->saved_symbol;
}

/*!re2c
 re2c:api = custom;
 re2c:api:style = free-form;
 re2c:define:YYCTYPE  = char;
 re2c:define:YYLESSTHAN = "parser->cursor >= parser->limit";
 re2c:define:YYPEEK = "*parser->cursor";
 re2c:define:YYSKIP = "re2c_yyskip (parser);";
 re2c:define:YYBACKUP = "re2c_yybackup (parser);";
 re2c:define:YYRESTORE = "re2c_yyrestore (parser);";
 re2c:define:YYFILL   = "re2c_refill_buffer (parser) == 0";
 re2c:define:YYSTAGP = "@@{tag} = parser->cursor;";
 re2c:define:YYSTAGN = "@@{tag} = NULL;";
 re2c:define:YYSHIFTSTAG  = "@@{tag} += @@{shift};";
 re2c:eof = 0;
 re2c:tags = 1;
 re2c:tags:expression = "parser->tags.@@";

 separator = [\x20\x0c\x0a\x0d\x09\x0b];
 identifier = [A-Za-z_][A-Za-z0-9_]*;
 comment = "//" .* "\n";

 string_literal_block = "\"" (. | "\\\"")* "\"";
 string_literal = string_literal_block ((separator | comment)* string_literal_block)*;
 integer_literal = ("+" | "-")? [0-9]+;
 floating_literal = ("+" | "-")? [0-9]* "." [0-9]+;
 */

static enum kan_readable_data_parser_response_t re2c_parse_first_value (struct parser_t *parser);

static enum kan_readable_data_parser_response_t re2c_parse_next_identifier_value (struct parser_t *parser);

static enum kan_readable_data_parser_response_t re2c_parse_next_string_value (struct parser_t *parser);

static enum kan_readable_data_parser_response_t re2c_parse_next_integer_value (struct parser_t *parser);

static enum kan_readable_data_parser_response_t re2c_parse_next_floating_value (struct parser_t *parser);

static inline const char *re2c_internalize_identifier (struct parser_t *parser, const char *begin, const char *end)
{
    const uint64_t length = end - begin;
    char *copy = kan_stack_group_allocator_allocate (&parser->temporary_allocator, length + 1u, _Alignof (char));
    memcpy (copy, begin, length);
    copy[length] = '\0';
    return copy;
}

struct string_literal_block_node_t
{
    struct string_literal_block_node_t *next;
    const char *begin;
    const char *end;
};

static inline const char *re2c_internalize_string_literal (struct parser_t *parser,
                                                           const char *blocks_begin,
                                                           const char *blocks_end)
{
    struct string_literal_block_node_t *first_block = NULL;
    struct string_literal_block_node_t *last_block = NULL;
    uint64_t total_length = 0u;

    while (blocks_begin != blocks_end)
    {
        if (*blocks_begin == '"')
        {
            struct string_literal_block_node_t *new_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                &parser->temporary_allocator, struct string_literal_block_node_t);
            ++blocks_begin;
            KAN_ASSERT (blocks_begin < blocks_end)
            new_node->begin = blocks_begin;

            while (KAN_TRUE)
            {
                if (*blocks_begin == '"' && *(blocks_begin - 1u) != '\\')
                {
                    new_node->end = blocks_begin;
                    ++blocks_begin;
                    break;
                }

                KAN_ASSERT (blocks_begin < blocks_end)
                ++blocks_begin;
            }

            total_length += new_node->end - new_node->begin;
            new_node->next = NULL;

            if (last_block)
            {
                last_block->next = new_node;
                last_block = new_node;
            }
            else
            {
                first_block = new_node;
                last_block = new_node;
            }
        }
        else
        {
            ++blocks_begin;
        }
    }

    char *copy = kan_stack_group_allocator_allocate (&parser->temporary_allocator, total_length + 1u, _Alignof (char));
    struct string_literal_block_node_t *block = first_block;
    char *output = copy;

    while (block)
    {
        if (block->end != block->begin)
        {
            memcpy (output, block->begin, block->end - block->begin);
            output += block->end - block->begin;
        }

        block = block->next;
    }

    copy[total_length] = '\0';
    return copy;
}

static inline int64_t re2c_parse_integer (const char *begin, const char *end)
{
    int64_t result = 0u;
    kan_bool_t positive = KAN_TRUE;

    if (*begin == '-')
    {
        positive = KAN_FALSE;
        ++begin;
    }
    else if (*begin == '+')
    {
        ++begin;
    }

    while (begin < end)
    {
        int64_t digit = *begin - '0';
        KAN_ASSERT (digit >= 0 && digit <= 9)
        result = result * 10u + digit;
        ++begin;
    }

    if (!positive)
    {
        KAN_ASSERT (result >= 0)
        result = -result;
    }

    return result;
}

static inline double re2c_parse_floating (const char *begin, const char *end)
{
    double result = 0.0;
    kan_bool_t positive = KAN_TRUE;

    if (*begin == '-')
    {
        positive = KAN_FALSE;
        ++begin;
    }
    else if (*begin == '+')
    {
        ++begin;
    }

    while (begin < end)
    {
        if (*begin == '.')
        {
            ++begin;
            break;
        }

        int digit = *begin - '0';
        KAN_ASSERT (digit >= 0 && digit <= 9)
        result = result * 10.0 + (double) digit;
        ++begin;
    }

    double after_point_modifier = 0.1;
    while (begin < end)
    {
        int digit = *begin - '0';
        KAN_ASSERT (digit >= 0 && digit <= 9)
        result = result + ((double) digit) * after_point_modifier;
        after_point_modifier *= 0.1;
        ++begin;
    }

    if (!positive)
    {
        result = -result;
    }

    return result;
}

static inline void re2c_save_output_target_to_event (struct parser_t *parser,
                                                     struct kan_readable_data_output_target_t *output_target,
                                                     const char *output_target_identifier_begin,
                                                     const char *output_target_identifier_end,
                                                     const char *output_target_array_index_begin,
                                                     const char *output_target_array_index_end)
{
    output_target->identifier =
        re2c_internalize_identifier (parser, output_target_identifier_begin, output_target_identifier_end);

    if (output_target_array_index_begin)
    {
        int64_t parsed_index = re2c_parse_integer (output_target_array_index_begin, output_target_array_index_end);
        KAN_ASSERT (parsed_index >= 0u)
        output_target->array_index = parsed_index;
    }
    else
    {
        output_target->array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE;
    }
}

static enum kan_readable_data_parser_response_t re2c_verify_blocks_on_input_end (struct parser_t *parser)
{
    if (parser->opened_blocks > 0u)
    {
        KAN_LOG (readable_data, KAN_LOG_ERROR,
                 "Error. [%ld:%ld]: Encountered end of input, but not all opened blocks are closed. Parser: %s. "
                 "Symbol code: 0x%x.\n",
                 (long) parser->cursor_line, (long) parser->cursor_symbol, __func__, (int) *parser->cursor);
        return KAN_READABLE_DATA_PARSER_RESPONSE_FAILED;
    }

    return KAN_READABLE_DATA_PARSER_RESPONSE_COMPLETED;
}

static enum kan_readable_data_parser_response_t re2c_parse_next_event (struct parser_t *parser)
{
    while (KAN_TRUE)
    {
        parser->token = parser->cursor;
        const char *output_target_identifier_begin = NULL;
        const char *output_target_identifier_end = NULL;
        const char *output_target_array_index_begin = NULL;
        const char *output_target_array_index_end = NULL;

        /*!re2c
         output_target = @output_target_identifier_begin identifier ("." identifier)* @output_target_identifier_end
                         ("[" @output_target_array_index_begin [0-9]+ @output_target_array_index_end "]")?;

         output_target separator* "=" separator*
         {
             re2c_save_output_target_to_event (parser, &parser->current_event.output_target,
                     output_target_identifier_begin, output_target_identifier_end, output_target_array_index_begin,
                     output_target_array_index_end);
             return re2c_parse_first_value (parser);
         }

         output_target separator* "{"
         {
             parser->current_event.type = KAN_READABLE_DATA_EVENT_STRUCTURAL_SETTER_BEGIN;
             re2c_save_output_target_to_event (parser, &parser->current_event.output_target,
                     output_target_identifier_begin, output_target_identifier_end, output_target_array_index_begin,
                     output_target_array_index_end);
             ++parser->opened_blocks;
             return KAN_READABLE_DATA_PARSER_RESPONSE_NEW_EVENT;
         }

         "+" output_target separator* "{"
         {
             parser->current_event.type = KAN_READABLE_DATA_EVENT_ARRAY_APPENDER_BEGIN;
             re2c_save_output_target_to_event (parser, &parser->current_event.output_target,
                     output_target_identifier_begin, output_target_identifier_end, output_target_array_index_begin,
                     output_target_array_index_end);
             ++parser->opened_blocks;
             return KAN_READABLE_DATA_PARSER_RESPONSE_NEW_EVENT;
         }

         "}"
         {
             if (parser->opened_blocks == 0u)
             {
                 KAN_LOG (readable_data, KAN_LOG_ERROR,
                          "Error. [%ld:%ld]: Encountered block end with no opened blocks. Parser: %s. "
                          "Symbol code: 0x%x.\n",
                          (long) parser->cursor_line, (long) parser->cursor_symbol, __func__, (int) *parser->cursor);
                 return KAN_READABLE_DATA_PARSER_RESPONSE_FAILED;
             }

             --parser->opened_blocks;
             parser->current_event.type = KAN_READABLE_DATA_EVENT_BLOCK_END;
             return KAN_READABLE_DATA_PARSER_RESPONSE_NEW_EVENT;
         }

         separator+ { continue; }

         comment { continue; }

         *
         {
             KAN_LOG (readable_data, KAN_LOG_ERROR,
                      "Error. [%ld:%ld]: Unable to parse next token. Parser: %s. Symbol code: 0x%x.\n",
                      (long) parser->cursor_line, (long) parser->cursor_symbol, __func__, (int) *parser->cursor);
             return KAN_READABLE_DATA_PARSER_RESPONSE_FAILED;
         }

         $ { return re2c_verify_blocks_on_input_end (parser); }
         */
    }
}

static inline void re2c_add_value_node (struct parser_t *parser, struct kan_readable_data_value_node_t *new_node)
{
    new_node->next = NULL;
    if (parser->current_event.setter_value_first == NULL)
    {
        parser->current_event.setter_value_first = new_node;
    }
    else
    {
        // Not very efficient, but okay, because we're dealing with very small arrays almost always.
        struct kan_readable_data_value_node_t *last_node = parser->current_event.setter_value_first;

        while (last_node->next != NULL)
        {
            last_node = last_node->next;
        }

        last_node->next = new_node;
    }
}

static inline void re2c_add_identifier_node (struct parser_t *parser,
                                             const char *literal_begin,
                                             const char *literal_end)
{
    struct kan_readable_data_value_node_t *new_node =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->temporary_allocator, struct kan_readable_data_value_node_t);
    new_node->identifier = re2c_internalize_identifier (parser, literal_begin, literal_end);
    re2c_add_value_node (parser, new_node);
}

static inline void re2c_add_string_node (struct parser_t *parser, const char *literal_begin, const char *literal_end)
{
    struct kan_readable_data_value_node_t *new_node =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->temporary_allocator, struct kan_readable_data_value_node_t);
    new_node->string = re2c_internalize_string_literal (parser, literal_begin, literal_end);
    re2c_add_value_node (parser, new_node);
}

static inline void re2c_add_integer_node (struct parser_t *parser, const char *literal_begin, const char *literal_end)
{
    struct kan_readable_data_value_node_t *new_node =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->temporary_allocator, struct kan_readable_data_value_node_t);
    new_node->integer = re2c_parse_integer (literal_begin, literal_end);
    re2c_add_value_node (parser, new_node);
}

static inline void re2c_add_floating_node (struct parser_t *parser, const char *literal_begin, const char *literal_end)
{
    struct kan_readable_data_value_node_t *new_node =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&parser->temporary_allocator, struct kan_readable_data_value_node_t);
    new_node->floating = re2c_parse_floating (literal_begin, literal_end);
    re2c_add_value_node (parser, new_node);
}

static enum kan_readable_data_parser_response_t re2c_parse_first_value (struct parser_t *parser)
{
    while (KAN_TRUE)
    {
        parser->token = parser->cursor;
        const char *literal_begin = NULL;
        const char *literal_end = NULL;

        /*!re2c
         @literal_begin identifier @literal_end
         {
             parser->current_event.type = KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER;
             parser->current_event.setter_value_first = NULL;
             re2c_add_identifier_node (parser, literal_begin, literal_end);
             return re2c_parse_next_identifier_value (parser);
         }

         @literal_begin string_literal @literal_end
         {
             parser->current_event.type = KAN_READABLE_DATA_EVENT_ELEMENTAL_STRING_SETTER;
             parser->current_event.setter_value_first = NULL;
             re2c_add_string_node (parser, literal_begin, literal_end);
             return re2c_parse_next_string_value (parser);
         }

         @literal_begin integer_literal @literal_end
         {
             parser->current_event.type = KAN_READABLE_DATA_EVENT_ELEMENTAL_INTEGER_SETTER;
             parser->current_event.setter_value_first = NULL;
             re2c_add_integer_node (parser, literal_begin, literal_end);
             return re2c_parse_next_integer_value (parser);
         }

         @literal_begin floating_literal @literal_end
         {
             parser->current_event.type = KAN_READABLE_DATA_EVENT_ELEMENTAL_FLOATING_SETTER;
             parser->current_event.setter_value_first = NULL;
             re2c_add_floating_node (parser, literal_begin, literal_end);
             return re2c_parse_next_floating_value (parser);
         }

         separator+ { continue; }

         comment { continue; }

         *
         {
             KAN_LOG (readable_data, KAN_LOG_ERROR,
                      "Error. [%ld:%ld]: Unable to parse next token. Parser: %s. Symbol code: 0x%x.\n",
                      (long) parser->cursor_line, (long) parser->cursor_symbol, __func__, (int) *parser->cursor);
             return KAN_READABLE_DATA_PARSER_RESPONSE_FAILED;
         }

         $
         {
             // Still have new event to present even if input is completed.
             return KAN_READABLE_DATA_PARSER_RESPONSE_NEW_EVENT;
         }
         */
    }
}

static enum kan_readable_data_parser_response_t re2c_parse_next_identifier_value (struct parser_t *parser)
{
    while (KAN_TRUE)
    {
        parser->token = parser->cursor;
        const char *literal_begin = NULL;
        const char *literal_end = NULL;
        re2c_save_cursor (parser);

        /*!re2c
         "," separator* @literal_begin identifier @literal_end
         {
             re2c_add_identifier_node (parser, literal_begin, literal_end);
             continue;
         }

         separator+ { continue; }

         comment { continue; }

         *
         {
             // We've reached end of setter, it is okay.
             re2c_restore_saved_cursor (parser);
             return KAN_READABLE_DATA_PARSER_RESPONSE_NEW_EVENT;
         }

         $
         {
             // Still have new event to present even if input is completed.
             return KAN_READABLE_DATA_PARSER_RESPONSE_NEW_EVENT;
         }
         */
    }
}

static enum kan_readable_data_parser_response_t re2c_parse_next_string_value (struct parser_t *parser)
{
    while (KAN_TRUE)
    {
        parser->token = parser->cursor;
        const char *literal_begin = NULL;
        const char *literal_end = NULL;
        re2c_save_cursor (parser);

        /*!re2c
         "," separator* @literal_begin string_literal @literal_end
         {
             re2c_add_string_node (parser, literal_begin, literal_end);
             continue;
         }

         separator+ { continue; }

         comment { continue; }

         *
         {
             // We've reached end of setter, it is okay.
             re2c_restore_saved_cursor (parser);
             return KAN_READABLE_DATA_PARSER_RESPONSE_NEW_EVENT;
         }

         $
         {
             // Still have new event to present even if input is completed.
             return KAN_READABLE_DATA_PARSER_RESPONSE_NEW_EVENT;
         }
         */
    }
}

static enum kan_readable_data_parser_response_t re2c_parse_next_integer_value (struct parser_t *parser)
{
    while (KAN_TRUE)
    {
        parser->token = parser->cursor;
        const char *literal_begin = NULL;
        const char *literal_end = NULL;
        re2c_save_cursor (parser);

        /*!re2c
         "," separator* @literal_begin integer_literal @literal_end
         {
             re2c_add_integer_node (parser, literal_begin, literal_end);
             continue;
         }

         separator+ { continue; }

         comment { continue; }

         *
         {
             // We've reached end of setter, it is okay.
             re2c_restore_saved_cursor (parser);
             return KAN_READABLE_DATA_PARSER_RESPONSE_NEW_EVENT;
         }

         $
         {
             // Still have new event to present even if input is completed.
             return KAN_READABLE_DATA_PARSER_RESPONSE_NEW_EVENT;
         }
         */
    }
}

static enum kan_readable_data_parser_response_t re2c_parse_next_floating_value (struct parser_t *parser)
{
    while (KAN_TRUE)
    {
        parser->token = parser->cursor;
        const char *literal_begin = NULL;
        const char *literal_end = NULL;
        re2c_save_cursor (parser);

        /*!re2c
         "," separator* @literal_begin floating_literal @literal_end
         {
             re2c_add_floating_node (parser, literal_begin, literal_end);
             continue;
         }

         separator+ { continue; }

         comment { continue; }

         *
         {
             // We've reached end of setter, it is okay.
             re2c_restore_saved_cursor (parser);
             return KAN_READABLE_DATA_PARSER_RESPONSE_NEW_EVENT;
         }

         $
         {
             // Still have new event to present even if input is completed.
             return KAN_READABLE_DATA_PARSER_RESPONSE_NEW_EVENT;
         }
         */
    }
}

kan_readable_data_parser_t kan_readable_data_parser_create (struct kan_stream_t *input_stream)
{
    ensure_statics_initialized ();
    KAN_ASSERT (kan_stream_is_readable (input_stream))

    struct parser_t *parser = (struct parser_t *) kan_allocate_general (
        readable_data_allocation_group, sizeof (struct parser_t), _Alignof (struct parser_t));

    parser->stream = input_stream;
    kan_stack_group_allocator_init (&parser->temporary_allocator, readable_data_temporary_allocation_group,
                                    KAN_READABLE_DATA_PARSE_TEMPORARY_ALLOCATOR_SIZE);

    parser->limit = parser->input_buffer + KAN_READABLE_DATA_PARSE_INPUT_BUFFER_SIZE - 1u;
    parser->cursor = parser->input_buffer + KAN_READABLE_DATA_PARSE_INPUT_BUFFER_SIZE - 1u;
    parser->marker = parser->input_buffer + KAN_READABLE_DATA_PARSE_INPUT_BUFFER_SIZE - 1u;
    parser->token = parser->input_buffer + KAN_READABLE_DATA_PARSE_INPUT_BUFFER_SIZE - 1u;
    *parser->limit = '\0';

    parser->end_of_input_reached = KAN_FALSE;
    parser->cursor_line = 1u;
    parser->cursor_symbol = 1u;
    parser->marker_line = 1u;
    parser->marker_symbol = 1u;

    parser->saved = parser->cursor;
    parser->saved_line = parser->cursor_line;
    parser->saved_symbol = parser->cursor_symbol;

    parser->opened_blocks = 0u;
    return (kan_readable_data_parser_t) parser;
}

enum kan_readable_data_parser_response_t kan_readable_data_parser_step (kan_readable_data_parser_t parser)
{
    struct parser_t *data = (struct parser_t *) parser;
    // Reset previous temporary allocations.
    kan_stack_group_allocator_reset (&data->temporary_allocator);
    return re2c_parse_next_event (data);
}

const struct kan_readable_data_event_t *kan_readable_data_parser_get_last_event (kan_readable_data_parser_t parser)
{
    struct parser_t *data = (struct parser_t *) parser;
    return &data->current_event;
}

void kan_readable_data_parser_destroy (kan_readable_data_parser_t parser)
{
    struct parser_t *data = (struct parser_t *) parser;
    kan_stack_group_allocator_shutdown (&data->temporary_allocator);
    kan_free_general (readable_data_allocation_group, data, sizeof (struct parser_t));
}

kan_readable_data_emitter_t kan_readable_data_emitter_create (struct kan_stream_t *output_stream)
{
    ensure_statics_initialized ();
    KAN_ASSERT (kan_stream_is_writeable (output_stream))

    struct emitter_t *emitter =
        kan_allocate_general (readable_data_allocation_group, sizeof (struct emitter_t), _Alignof (struct emitter_t));
    emitter->stream = output_stream;
    emitter->indentation_level = 0u;
    return (kan_readable_data_emitter_t) emitter;
}

static inline kan_bool_t emit_indentation (struct emitter_t *emitter)
{
#define INDENTATION "    "
#define INDENTATION_LENGTH 4u
    for (uint64_t index = 0u; index < emitter->indentation_level; ++index)
    {
        if (emitter->stream->operations->write (emitter->stream, INDENTATION_LENGTH, INDENTATION) != INDENTATION_LENGTH)
        {
            return KAN_FALSE;
        }
    }
#undef INDENTATION_LENGTH
#undef INDENTATION

    return KAN_TRUE;
}

static inline kan_bool_t emit_end_of_line (struct emitter_t *emitter)
{
    return emitter->stream->operations->write (emitter->stream, 1u, "\n") == 1u;
}

static inline kan_bool_t emit_identifier (struct emitter_t *emitter, const char *identifier)
{
    const uint64_t identifier_length = strlen (identifier);
    return emitter->stream->operations->write (emitter->stream, identifier_length, identifier) == identifier_length;
}

static inline kan_bool_t emit_output_target (struct emitter_t *emitter,
                                             struct kan_readable_data_output_target_t *target)
{
    if (!emit_identifier (emitter, target->identifier))
    {
        return KAN_FALSE;
    }

    if (target->array_index != KAN_READABLE_DATA_ARRAY_INDEX_NONE)
    {
        if (emitter->stream->operations->write (emitter->stream, 1u, "[") != 1u)
        {
            return KAN_FALSE;
        }

        const uint64_t formatted_length =
            (uint64_t) snprintf (emitter->formatting_buffer, KAN_READABLE_DATA_EMIT_FORMATTING_BUFFER_SIZE, "%llu",
                                 (unsigned long long) target->array_index);

        if (emitter->stream->operations->write (emitter->stream, formatted_length, emitter->formatting_buffer) !=
            formatted_length)
        {
            return KAN_FALSE;
        }

        if (emitter->stream->operations->write (emitter->stream, 1u, "]") != 1u)
        {
            return KAN_FALSE;
        }
    }

    return KAN_TRUE;
}

static inline kan_bool_t emit_string_literal (struct emitter_t *emitter, const char *literal)
{
    if (emitter->stream->operations->write (emitter->stream, 1u, "\"") != 1u)
    {
        return KAN_FALSE;
    }

    const uint64_t string_length = strlen (literal);
    if (emitter->stream->operations->write (emitter->stream, string_length, literal) != string_length)
    {
        return KAN_FALSE;
    }

    if (emitter->stream->operations->write (emitter->stream, 1u, "\"") != 1u)
    {
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline kan_bool_t emit_integer_literal (struct emitter_t *emitter, int64_t literal)
{
    const uint64_t formatted_length = (uint64_t) snprintf (
        emitter->formatting_buffer, KAN_READABLE_DATA_EMIT_FORMATTING_BUFFER_SIZE, "%lld", (signed long long) literal);

    return emitter->stream->operations->write (emitter->stream, formatted_length, emitter->formatting_buffer) ==
           formatted_length;
}

static inline kan_bool_t emit_floating_literal (struct emitter_t *emitter, double literal)
{
    const uint64_t formatted_length =
        (uint64_t) snprintf (emitter->formatting_buffer, KAN_READABLE_DATA_EMIT_FORMATTING_BUFFER_SIZE, "%lf", literal);

    return emitter->stream->operations->write (emitter->stream, formatted_length, emitter->formatting_buffer) ==
           formatted_length;
}

kan_bool_t kan_readable_data_emitter_step (kan_readable_data_emitter_t emitter,
                                           struct kan_readable_data_event_t *emit_event)
{
    struct emitter_t *data = (struct emitter_t *) emitter;
    switch (emit_event->type)
    {
    case KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER:
    {
        if (!emit_event->setter_value_first)
        {
            return KAN_FALSE;
        }

        if (!emit_indentation (data))
        {
            return KAN_FALSE;
        }

        if (!emit_output_target (data, &emit_event->output_target))
        {
            return KAN_FALSE;
        }

        if (data->stream->operations->write (data->stream, 3u, " = ") != 3u)
        {
            return KAN_FALSE;
        }

        struct kan_readable_data_value_node_t *node = emit_event->setter_value_first;
        while (node)
        {
            if (node != emit_event->setter_value_first)
            {
                if (data->stream->operations->write (data->stream, 2u, ", ") != 2u)
                {
                    return KAN_FALSE;
                }
            }

            if (!emit_identifier (data, node->identifier))
            {
                return KAN_FALSE;
            }

            node = node->next;
        }

        break;
    }

    case KAN_READABLE_DATA_EVENT_ELEMENTAL_STRING_SETTER:
    {
        if (!emit_event->setter_value_first)
        {
            return KAN_FALSE;
        }

        if (!emit_indentation (data))
        {
            return KAN_FALSE;
        }

        if (!emit_output_target (data, &emit_event->output_target))
        {
            return KAN_FALSE;
        }

        if (data->stream->operations->write (data->stream, 3u, " = ") != 3u)
        {
            return KAN_FALSE;
        }

        struct kan_readable_data_value_node_t *node = emit_event->setter_value_first;
        while (node)
        {
            if (node != emit_event->setter_value_first)
            {
                if (data->stream->operations->write (data->stream, 2u, ", ") != 2u)
                {
                    return KAN_FALSE;
                }
            }

            if (!emit_string_literal (data, node->string))
            {
                return KAN_FALSE;
            }

            node = node->next;
        }

        break;
    }

    case KAN_READABLE_DATA_EVENT_ELEMENTAL_INTEGER_SETTER:
    {
        if (!emit_event->setter_value_first)
        {
            return KAN_FALSE;
        }

        if (!emit_indentation (data))
        {
            return KAN_FALSE;
        }

        if (!emit_output_target (data, &emit_event->output_target))
        {
            return KAN_FALSE;
        }

        if (data->stream->operations->write (data->stream, 3u, " = ") != 3u)
        {
            return KAN_FALSE;
        }

        struct kan_readable_data_value_node_t *node = emit_event->setter_value_first;
        while (node)
        {
            if (node != emit_event->setter_value_first)
            {
                if (data->stream->operations->write (data->stream, 2u, ", ") != 2u)
                {
                    return KAN_FALSE;
                }
            }

            if (!emit_integer_literal (data, node->integer))
            {
                return KAN_FALSE;
            }

            node = node->next;
        }

        break;
    }

    case KAN_READABLE_DATA_EVENT_ELEMENTAL_FLOATING_SETTER:
    {
        if (!emit_event->setter_value_first)
        {
            return KAN_FALSE;
        }

        if (!emit_indentation (data))
        {
            return KAN_FALSE;
        }

        if (!emit_output_target (data, &emit_event->output_target))
        {
            return KAN_FALSE;
        }

        if (data->stream->operations->write (data->stream, 3u, " = ") != 3u)
        {
            return KAN_FALSE;
        }

        struct kan_readable_data_value_node_t *node = emit_event->setter_value_first;
        while (node)
        {
            if (node != emit_event->setter_value_first)
            {
                if (data->stream->operations->write (data->stream, 2u, ", ") != 2u)
                {
                    return KAN_FALSE;
                }
            }

            if (!emit_floating_literal (data, node->floating))
            {
                return KAN_FALSE;
            }

            node = node->next;
        }

        break;
    }

    case KAN_READABLE_DATA_EVENT_STRUCTURAL_SETTER_BEGIN:
        if (!emit_indentation (data))
        {
            return KAN_FALSE;
        }

        if (!emit_output_target (data, &emit_event->output_target))
        {
            return KAN_FALSE;
        }

        if (data->stream->operations->write (data->stream, 2u, " {") != 2u)
        {
            return KAN_FALSE;
        }

        ++data->indentation_level;
        break;

    case KAN_READABLE_DATA_EVENT_ARRAY_APPENDER_BEGIN:
        if (!emit_indentation (data))
        {
            return KAN_FALSE;
        }

        if (data->stream->operations->write (data->stream, 1u, "+") != 1u)
        {
            return KAN_FALSE;
        }

        if (!emit_output_target (data, &emit_event->output_target))
        {
            return KAN_FALSE;
        }

        if (data->stream->operations->write (data->stream, 2u, " {") != 2u)
        {
            return KAN_FALSE;
        }

        ++data->indentation_level;
        break;

    case KAN_READABLE_DATA_EVENT_BLOCK_END:
        if (data->indentation_level == 0u)
        {
            return KAN_FALSE;
        }

        --data->indentation_level;
        if (!emit_indentation (data))
        {
            return KAN_FALSE;
        }

        if (data->stream->operations->write (data->stream, 1u, "}") != 1u)
        {
            return KAN_FALSE;
        }

        break;
    }

    return emit_end_of_line (data);
}

void kan_readable_data_emitter_destroy (kan_readable_data_emitter_t emitter)
{
    struct emitter_t *data = (struct emitter_t *) emitter;
    kan_free_general (readable_data_allocation_group, data, sizeof (struct emitter_t));
}
