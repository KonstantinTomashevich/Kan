#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/min_max.h>
#include <kan/c_interface/builder.h>
#include <kan/c_interface/file.h>
#include <kan/container/trivial_string_buffer.h>
#include <kan/error/critical.h>
#include <kan/file_system/stream.h>
#include <kan/memory/allocation.h>

#define RETURN_CODE_SUCCESS 0
#define RETURN_CODE_INVALID_ARGUMENTS (-1)
#define RETURN_CODE_UNABLE_TO_OPEN_INPUT (-2)
#define RETURN_CODE_PARSE_FAILED (-3)
#define RETURN_CODE_BUILD_FAILED (-4)
#define RETURN_CODE_UNABLE_TO_OPEN_OUTPUT (-5)
#define RETURN_CODE_SERIALIZATION_FAILED (-6)

#define INPUT_ERROR_REFIL_AFTER_END_OF_FILE 1
#define INPUT_ERROR_LEXEME_OVERFLOW 2

// Global state.
static struct
{
    const char *export_macro;
    const char *input_file_path;
    const char *output_file_path;
} arguments;

#define INPUT_BUFFER_SIZE 131072u // 128 kilobytes

struct tags_t
{
    /*!stags:re2c format = 'const char *@@;';*/
};

static struct
{
    struct kan_stream_t *input_stream;
    char input_buffer[INPUT_BUFFER_SIZE];
    char *limit;
    char *cursor;
    char *marker;
    char *token;

    kan_bool_t end_of_input_reached;
    size_t cursor_line;
    size_t cursor_symbol;
    size_t marker_line;
    size_t marker_symbol;

    struct tags_t tags;
} io = {
    .input_stream = NULL,
    .input_buffer = {0},
    .limit = io.input_buffer + INPUT_BUFFER_SIZE - 1u,
    .cursor = io.input_buffer + INPUT_BUFFER_SIZE - 1u,
    .marker = io.input_buffer + INPUT_BUFFER_SIZE - 1u,
    .token = io.input_buffer + INPUT_BUFFER_SIZE - 1u,
    .end_of_input_reached = KAN_FALSE,
    .cursor_line = 1u,
    .cursor_symbol = 1u,
    .marker_line = 1u,
    .marker_symbol = 1u,
};

static struct
{
    struct kan_c_token_t *first;
    struct kan_c_token_t *last;
} reporting = {
    .first = NULL,
    .last = NULL,
};

static struct kan_c_interface_file_t interface_file;
static kan_bool_t interface_file_should_have_includable_object;
static struct kan_trivial_string_buffer_t optional_includable_object_buffer;

// IO

static int io_refill_buffer (void)
{
    if (io.end_of_input_reached)
    {
        return INPUT_ERROR_REFIL_AFTER_END_OF_FILE;
    }

    const size_t shift = io.token - io.input_buffer;
    const size_t used = io.limit - io.token;

    if (shift < 1)
    {
        return INPUT_ERROR_LEXEME_OVERFLOW;
    }

    // Shift buffer contents (discard everything up to the current token).
    memmove (io.input_buffer, io.token, used);
    io.limit -= shift;
    io.cursor -= shift;
    io.marker -= shift;
    io.token -= shift;

    const char **first_tag = (const char **) &io.tags;
    const char **last_tag = first_tag + sizeof (struct tags_t) / sizeof (char *);

    while (first_tag != last_tag)
    {
        if (*first_tag)
        {
            *first_tag -= shift;
        }

        ++first_tag;
    }

    // Fill free space at the end of buffer with new data from file.
    io.limit += io.input_stream->operations->read (io.input_stream, INPUT_BUFFER_SIZE - used - 1u, io.limit);
    io.limit[0u] = 0;
    io.end_of_input_reached = io.limit < io.input_buffer + INPUT_BUFFER_SIZE - 1u;

    return 0;
}

// Parse result reporting functions

static struct kan_c_token_t *create_next_token (void)
{
    struct kan_c_token_t *token = kan_allocate_batched (KAN_ALLOCATION_GROUP_IGNORE, sizeof (struct kan_c_token_t));
    token->next = NULL;

    if (reporting.last)
    {
        reporting.last->next = token;
        reporting.last = token;
    }
    else
    {
        reporting.first = token;
        reporting.last = token;
    }

    return token;
}

static void report_meta_marker (const char *name_begin, const char *name_end)
{
    struct kan_c_token_t *token = create_next_token ();
    token->type = KAN_C_TOKEN_MARKER_META;
    token->marker_meta.name = kan_char_sequence_intern (name_begin, name_end);
}

static void report_meta_integer (const char *name_begin,
                                 const char *name_end,
                                 const char *value_begin,
                                 const char *value_end)
{
    struct kan_c_token_t *token = create_next_token ();
    token->type = KAN_C_TOKEN_INTEGER_META;
    token->integer_meta.name = kan_char_sequence_intern (name_begin, name_end);

    int64_t sign = 1;
    if (*value_begin == '-')
    {
        sign = -1;
        ++value_begin;
    }
    else if (*value_begin == '+')
    {
        ++value_begin;
    }
    else
    {
        KAN_ASSERT (isdigit (*value_begin))
    }

    int64_t number = 0;
    while (value_begin != value_end)
    {
        KAN_ASSERT (isdigit (*value_begin))
        number = number * 10 + (*value_begin - '0');
        ++value_begin;
    }

    token->integer_meta.value = sign * number;
}

static void report_meta_string (const char *name_begin,
                                const char *name_end,
                                const char *value_begin,
                                const char *value_end)
{
    struct kan_c_token_t *token = create_next_token ();
    token->type = KAN_C_TOKEN_STRING_META;
    token->string_meta.name = kan_char_sequence_intern (name_begin, name_end);
    token->string_meta.value = kan_char_sequence_intern (value_begin, value_end);
}

static void report_enum_begin (const char *name_begin, const char *name_end)
{
    struct kan_c_token_t *token = create_next_token ();
    token->type = KAN_C_TOKEN_ENUM_BEGIN;
    token->enum_begin.name = kan_char_sequence_intern (name_begin, name_end);
}

static void report_enum_value (const char *name_begin, const char *name_end)
{
    struct kan_c_token_t *token = create_next_token ();
    token->type = KAN_C_TOKEN_ENUM_VALUE;
    token->enum_value.name = kan_char_sequence_intern (name_begin, name_end);
}

static void report_enum_end (void)
{
    struct kan_c_token_t *token = create_next_token ();
    token->type = KAN_C_TOKEN_ENUM_END;
}

static void report_struct_begin (const char *name_begin, const char *name_end)
{
    struct kan_c_token_t *token = create_next_token ();
    token->type = KAN_C_TOKEN_STRUCT_BEGIN;
    token->struct_begin.name = kan_char_sequence_intern (name_begin, name_end);
}

static void report_struct_field (kan_bool_t is_const,
                                 enum kan_c_archetype_t archetype,
                                 const char *type_name_begin,
                                 const char *type_name_end,
                                 size_t pointer_level,
                                 const char *name_begin,
                                 const char *name_end,
                                 kan_bool_t is_array)
{
    struct kan_c_token_t *token = create_next_token ();
    token->type = KAN_C_TOKEN_STRUCT_FIELD;
    token->struct_field.name = kan_char_sequence_intern (name_begin, name_end);
    token->struct_field.type.name = kan_char_sequence_intern (type_name_begin, type_name_end);
    token->struct_field.type.archetype = archetype;
    token->struct_field.type.is_array = is_array;
    token->struct_field.type.is_const = is_const;

    KAN_ASSERT (pointer_level <= UINT8_MAX)
    token->struct_field.type.pointer_level = (uint8_t) pointer_level;
}

static void report_struct_end (void)
{
    struct kan_c_token_t *token = create_next_token ();
    token->type = KAN_C_TOKEN_STRUCT_END;
}

static void report_exported_function_begin (kan_bool_t is_return_const,
                                            enum kan_c_archetype_t return_archetype,
                                            const char *type_name_begin,
                                            const char *type_name_end,
                                            size_t pointer_level,
                                            const char *name_begin,
                                            const char *name_end)
{
    struct kan_c_token_t *token = create_next_token ();
    token->type = KAN_C_TOKEN_FUNCTION_BEGIN;
    token->function_begin.name = kan_char_sequence_intern (name_begin, name_end);
    token->function_begin.return_type.name = kan_char_sequence_intern (type_name_begin, type_name_end);
    token->function_begin.return_type.archetype = return_archetype;
    token->function_begin.return_type.is_array = KAN_FALSE;
    token->function_begin.return_type.is_const = is_return_const;

    KAN_ASSERT (pointer_level <= UINT8_MAX)
    token->function_begin.return_type.pointer_level = (uint8_t) pointer_level;
}

static void report_exported_function_argument (kan_bool_t is_const,
                                               enum kan_c_archetype_t archetype,
                                               const char *type_name_begin,
                                               const char *type_name_end,
                                               size_t pointer_level,
                                               const char *name_begin,
                                               const char *name_end)
{
    struct kan_c_token_t *token = create_next_token ();
    token->type = KAN_C_TOKEN_FUNCTION_ARGUMENT;
    token->function_argument.name = kan_char_sequence_intern (name_begin, name_end);
    token->function_argument.type.name = kan_char_sequence_intern (type_name_begin, type_name_end);
    token->function_argument.type.archetype = archetype;
    token->function_argument.type.is_array = KAN_FALSE;
    token->function_argument.type.is_const = is_const;

    KAN_ASSERT (pointer_level <= UINT8_MAX)
    token->struct_field.type.pointer_level = (uint8_t) pointer_level;
}

static void report_exported_function_end (void)
{
    struct kan_c_token_t *token = create_next_token ();
    token->type = KAN_C_TOKEN_FUNCTION_END;
}

static void report_exported_symbol (kan_bool_t is_const,
                                    enum kan_c_archetype_t archetype,
                                    const char *type_name_begin,
                                    const char *type_name_end,
                                    size_t pointer_level,
                                    const char *name_begin,
                                    const char *name_end,
                                    kan_bool_t is_array)
{
    struct kan_c_token_t *token = create_next_token ();
    token->type = KAN_C_TOKEN_SYMBOL;
    token->symbol.name = kan_char_sequence_intern (name_begin, name_end);
    token->symbol.type.name = kan_char_sequence_intern (type_name_begin, type_name_end);
    token->symbol.type.archetype = archetype;
    token->symbol.type.is_array = is_array;
    token->symbol.type.is_const = is_const;

    KAN_ASSERT (pointer_level <= UINT8_MAX)
    token->struct_field.type.pointer_level = (uint8_t) pointer_level;
}

// Optional includable object building

static void optional_includable_object_begin (void)
{
    if (!interface_file_should_have_includable_object)
    {
        return;
    }

    kan_trivial_string_buffer_init (&optional_includable_object_buffer, KAN_ALLOCATION_GROUP_IGNORE, INPUT_BUFFER_SIZE);
}

static void optional_includable_object_append_token (void)
{
    if (!interface_file_should_have_includable_object)
    {
        return;
    }

    kan_trivial_string_buffer_append_char_sequence (&optional_includable_object_buffer, io.token, io.cursor - io.token);
}

static void optional_includable_object_append_string (char *string)
{
    if (!interface_file_should_have_includable_object)
    {
        return;
    }

    kan_trivial_string_buffer_append_char_sequence (&optional_includable_object_buffer, string, strlen (string));
}

static void optional_includable_object_finish (void)
{
    if (!interface_file_should_have_includable_object || optional_includable_object_buffer.size == 0u)
    {
        return;
    }

    interface_file.optional_includable_object = kan_allocate_general (
        KAN_ALLOCATION_GROUP_IGNORE, optional_includable_object_buffer.size + 1u, _Alignof (char));
    strncpy (interface_file.optional_includable_object, optional_includable_object_buffer.buffer,
             optional_includable_object_buffer.size);

    interface_file.optional_includable_object[optional_includable_object_buffer.size] = '\0';
    kan_trivial_string_buffer_shutdown (&optional_includable_object_buffer);
}

// Parse input using re2c

// Helpers for re2c api.
static void re2c_yyskip (void)
{
    if (*io.cursor == '\n')
    {
        ++io.cursor_line;
        io.cursor_symbol = 0u;
    }

    ++io.cursor;
    ++io.cursor_symbol;
}

static void re2c_yybackup (void)
{
    io.marker = io.cursor;
    io.marker_line = io.cursor_line;
    io.marker_symbol = io.cursor_symbol;
}

static void re2c_yyrestore (void)
{
    io.cursor = io.marker;
    io.cursor_line = io.marker_line;
    io.cursor_symbol = io.marker_symbol;
}

// Define re2c api.
/*!re2c
 re2c:api = custom;
 re2c:api:style = free-form;
 re2c:define:YYCTYPE  = char;
 re2c:define:YYLESSTHAN = "io.cursor >= io.limit";
 re2c:define:YYPEEK = "*io.cursor";
 re2c:define:YYSKIP = "re2c_yyskip ();";
 re2c:define:YYBACKUP = "re2c_yybackup ();";
 re2c:define:YYRESTORE = "re2c_yyrestore ();";
 re2c:define:YYFILL   = "io_refill_buffer () == 0";
 re2c:define:YYSTAGP = "@@{tag} = io.cursor;";
 re2c:define:YYSTAGN = "@@{tag} = NULL;";
 re2c:define:YYSHIFTSTAG  = "@@{tag} += @@{shift};";
 re2c:eof = 0;
 re2c:tags = 1;
 re2c:tags:expression = "io.tags.@@";
*/

// Common captures.
static const char *capture_identifier_begin;
static const char *capture_identifier_end;

static const char *capture_type_name_begin;
static const char *capture_type_name_end;

static const char *capture_const_begin;
static const char *capture_const_end;

static const char *capture_enum_begin;
static const char *capture_enum_end;

static const char *capture_struct_begin;
static const char *capture_struct_end;

static const char *capture_pointer_begin;
static const char *capture_pointer_end;

static const char *capture_array_suffix_begin;
static const char *capture_array_suffix_end;

static const char *capture_meta_value_begin;
static const char *capture_meta_value_end;

// Define common matches.
/*!re2c
 separator = [\x20\x0c\x0a\x0d\x09\x0b];
 any_preprocessor = "#" ((.+) | (.*"\\\n") | (.*"\\\r\n"))*;

 identifier = @capture_identifier_begin [A-Za-z_][A-Za-z0-9_]* @capture_identifier_end;

 type = (@capture_const_begin "const" @capture_const_end separator+)?
         ((@capture_enum_begin "enum" @capture_enum_end separator+) |
         (@capture_struct_begin "struct" @capture_struct_end separator+))?
         @capture_type_name_begin [A-Za-z_][A-Za-z0-9_]* @capture_type_name_end
        separator? @capture_pointer_begin "*"* @capture_pointer_end;

 array_suffix = @capture_array_suffix_begin "[" .* "]" @capture_array_suffix_end;
 */

// Define common rules.

/*!rules:re2c:default
 "//" separator* "\\c_interface_scanner_disable" separator*
 { if (!parse_skip_until_enabled ()) { return KAN_FALSE; } continue; }
 "/""*" { if (!parse_subroutine_multi_line_comment ()) { return KAN_FALSE; } continue; }
 "//" { if (!parse_subroutine_single_line_comment ()) { return KAN_FALSE; } continue; }

 separator | any_preprocessor { optional_includable_object_append_token (); continue; }

 *
 {
     fprintf (stderr, "Error. [%ld:%ld]: Unable to parse next token. Parser: %s. Symbol code: 0x%x.\n",
         (long) io.cursor_line, (long) io.cursor_symbol, __func__, (int) *io.cursor);
     return KAN_FALSE;
 }
 */

/*!rules:re2c:skip_comments
 ("//".+) | ("/""*"(. | "\n")+"*""/") { continue; }
 */

/*!rules:re2c:parse_meta
 "\\meta" separator+ identifier
 {
     report_meta_marker (capture_identifier_begin, capture_identifier_end);
     continue;
 }

 "\\meta" separator+ identifier separator* "=" separator* @capture_meta_value_begin [+-]?[0-9]+ @capture_meta_value_end
 {
     report_meta_integer (
         capture_identifier_begin,
         capture_identifier_end,
         capture_meta_value_begin,
         capture_meta_value_end);
     continue;
 }

 "\\meta" separator+ identifier separator* "=" separator* "\"" @capture_meta_value_begin .* @capture_meta_value_end "\""
 {
     report_meta_string (
         capture_identifier_begin,
         capture_identifier_end,
         capture_meta_value_begin,
         capture_meta_value_end);
     continue;
 }

"\\meta" separator+ identifier separator* "=" separator* @capture_meta_value_begin [A-Za-z0-9_]+ @capture_meta_value_end
 {
     report_meta_string (
         capture_identifier_begin,
         capture_identifier_end,
         capture_meta_value_begin,
         capture_meta_value_end);
     continue;
 }
 */

static kan_bool_t parse_main (void);
static kan_bool_t parse_enum (void);
static kan_bool_t parse_struct (void);
static kan_bool_t parse_exported_symbol_begin (void);
static kan_bool_t parse_exported_function_arguments (void);
static kan_bool_t parse_skip_until_round_braces_close (kan_bool_t append_token);
static kan_bool_t parse_skip_until_curly_braces_close (void);

static kan_bool_t parse_skip_until_enabled (void);
static kan_bool_t parse_subroutine_multi_line_comment (void);
static kan_bool_t parse_subroutine_single_line_comment (void);

static kan_bool_t parse_main (void)
{
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         !use:default;

         // Some static global variable, skip it.
         "static" separator+ type separator* identifier separator* ("[".*"]")? ("=" | ";")
         {
             continue;
         }

         // Some static function, skip it.
         "static" separator+ type separator* identifier separator* "("
         {
             return parse_skip_until_round_braces_close (KAN_FALSE);
         }

         identifier (separator | [;])
         {
             optional_includable_object_append_token ();
             if (strncmp (
                 capture_identifier_begin, arguments.export_macro,
                 capture_identifier_end - capture_identifier_begin) == 0)
             {
                 return parse_exported_symbol_begin ();
             }
             else
             {
                 continue;
             }
         }

         // Named enum.
        "enum" separator+ identifier separator* "{"
        {
            optional_includable_object_append_token ();
            report_enum_begin (capture_identifier_begin, capture_identifier_end);
            return parse_enum ();
        }

         // Named structure.
         "struct" separator+ identifier separator* "{"
         {
             optional_includable_object_append_token ();
             report_struct_begin (capture_identifier_begin, capture_identifier_end);
             return parse_struct ();
         }

         // Forbidden typedef.
         "typedef" separator+ ("enum" | "struct") [^;]+ ";"
         {
             fprintf (stderr, "Error. [%ld:%ld]: Encountered struct/enum hiding typedef, it confuses parser.\n",
                (long) io.cursor_line, (long) io.cursor_symbol);
             return KAN_FALSE;
         }

         // We ignore usual typedefs.
         "typedef" separator+ [^;]+ ";" { optional_includable_object_append_token (); continue; }

         // Looks like we've encountered '(' from function that is not exported. Skip everything inside.
         "(" { optional_includable_object_append_token (); return parse_skip_until_round_braces_close (KAN_TRUE); }

         // Looks like we've encountered '{' from function body or initializer. Skip everything inside.
         "{" { optional_includable_object_append_string (";"); return parse_skip_until_curly_braces_close (); }

         // Symbols about which we're not concerned while they're in global scope.
         [*;+] { optional_includable_object_append_token (); continue; }
         "=" { continue; }
         "=" (separator | [a-zA-Z0-9+*_] | "-" | "/" | ")" | "(" )* ";"
         { optional_includable_object_append_string (";"); continue; }

         $ { return KAN_TRUE; }
         */
    }
}

static kan_bool_t parse_enum (void)
{
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         !use:default;

         identifier (separator? "=" separator* ([0-9a-zA-Z_+<>] | separator | "-")+ separator*)?
         {
             optional_includable_object_append_token ();
             report_enum_value (
                 capture_identifier_begin,
                 capture_identifier_end);
             continue;
         }

         "," { optional_includable_object_append_token (); continue; }

         "}" { optional_includable_object_append_token (); report_enum_end (); return parse_main (); }

         $ { fprintf (stderr, "Error. Reached end of file while parsing enum."); return KAN_FALSE; }
         */
    }
}

static kan_bool_t parse_struct (void)
{
    kan_bool_t inside_union = KAN_FALSE;
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         !use:default;

         type separator* identifier separator* array_suffix? separator* ";"
         {
             optional_includable_object_append_token ();
             report_struct_field (
                 capture_const_begin == capture_const_end ? KAN_FALSE : KAN_TRUE,
                 capture_enum_begin != capture_enum_end     ? KAN_C_ARCHETYPE_ENUM :
                 capture_struct_begin != capture_struct_end ? KAN_C_ARCHETYPE_STRUCT :
                                                              KAN_C_ARCHETYPE_BASIC,
                 capture_type_name_begin,
                 capture_type_name_end,
                 capture_pointer_end - capture_pointer_begin,
                 capture_identifier_begin,
                 capture_identifier_end,
                 capture_array_suffix_begin == capture_array_suffix_end ? KAN_FALSE : KAN_TRUE);
             continue;
         }

         "union" separator* "{"
         {
             optional_includable_object_append_token ();
             if (inside_union)
             {
                 fprintf (stderr, "Error. [%ld:%ld]: Nested unions aren't supported.\n",
                     (long) io.cursor_line, (long) io.cursor_symbol);
                 return KAN_FALSE;
             }

             inside_union = KAN_TRUE;
             continue;
         }

         ";" { optional_includable_object_append_token (); continue; }

         "}"
         {
             optional_includable_object_append_token ();
             if (inside_union)
             {
                 inside_union = KAN_FALSE;
                 continue;
             }

             report_struct_end ();
             return parse_main ();
         }

         $ { fprintf (stderr, "Error. Reached end of file while parsing structure."); return KAN_FALSE; }
         */
    }
}

static kan_bool_t parse_exported_symbol_begin (void)
{
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         !use:default;

         // Exported function
         type separator* identifier separator* "("
         {
             optional_includable_object_append_token ();
             report_exported_function_begin (
                 capture_const_begin == capture_const_end ? KAN_FALSE : KAN_TRUE,
                 capture_enum_begin != capture_enum_end     ? KAN_C_ARCHETYPE_ENUM :
                 capture_struct_begin != capture_struct_end ? KAN_C_ARCHETYPE_STRUCT :
                                                              KAN_C_ARCHETYPE_BASIC,
                 capture_type_name_begin,
                 capture_type_name_end,
                 capture_pointer_end - capture_pointer_begin,
                 capture_identifier_begin,
                 capture_identifier_end);
             return parse_exported_function_arguments ();
         }

         // Exported symbol
         type separator* identifier separator* array_suffix? separator* ";"*
         {
             optional_includable_object_append_string ("extern ");
             optional_includable_object_append_token ();
             optional_includable_object_append_string (";");
             report_exported_symbol (
                 capture_const_begin == capture_const_end ? KAN_FALSE : KAN_TRUE,
                 capture_enum_begin != capture_enum_end     ? KAN_C_ARCHETYPE_ENUM :
                 capture_struct_begin != capture_struct_end ? KAN_C_ARCHETYPE_STRUCT :
                                                              KAN_C_ARCHETYPE_BASIC,
                 capture_type_name_begin,
                 capture_type_name_end,
                 capture_pointer_end - capture_pointer_begin,
                 capture_identifier_begin,
                 capture_identifier_end,
                 capture_array_suffix_begin == capture_array_suffix_end ? KAN_FALSE : KAN_TRUE);
             return parse_main ();
         }

         $
         {
             fprintf (stderr, "Error. Reached end of file while expecting function or symbol declaration.");
             return KAN_FALSE;
         }
         */
    }
}

static kan_bool_t parse_exported_function_arguments (void)
{
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         !use:default;

         type separator* identifier separator*
         {
            optional_includable_object_append_token ();
            report_exported_function_argument (
                capture_const_begin == capture_const_end ? KAN_FALSE : KAN_TRUE,
                capture_enum_begin != capture_enum_end     ? KAN_C_ARCHETYPE_ENUM :
                capture_struct_begin != capture_struct_end ? KAN_C_ARCHETYPE_STRUCT :
                                                             KAN_C_ARCHETYPE_BASIC,
                capture_type_name_begin,
                capture_type_name_end,
                capture_pointer_end - capture_pointer_begin,
                capture_identifier_begin,
                capture_identifier_end);
            continue;
         }

         "," { optional_includable_object_append_token (); continue; }

         "void" { optional_includable_object_append_token (); continue; }

         ")"
         {
             optional_includable_object_append_token ();
             optional_includable_object_append_string (";");
             report_exported_function_end ();
             return parse_main ();
         }

         $ { fprintf (stderr, "Error. Reached end of file while parsing function arguments."); return KAN_FALSE; }
         */
    }
}

static kan_bool_t parse_skip_until_round_braces_close (kan_bool_t append_token)
{
    size_t left_to_close = 1u;
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         !use:skip_comments;

         "("
         {
             if (append_token)
             {
                 optional_includable_object_append_token ();
             }

             ++left_to_close;
             continue;
         }

         ")"
         {
             if (append_token)
             {
                 optional_includable_object_append_token ();
             }

             --left_to_close;
             if (left_to_close == 0u)
             {
                 return parse_main ();
             }

             continue;
         }

         * { if (append_token) { optional_includable_object_append_token (); } continue; }
         $ { fprintf (stderr, "Error. Reached end of file while waiting for round braces to close."); return KAN_FALSE;
         }
        */
    }
}

static kan_bool_t parse_skip_until_curly_braces_close (void)
{
    size_t left_to_close = 1u;
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         !use:skip_comments;

         "{"
         {
             ++left_to_close;
             continue;
         }

         "}"
         {
             --left_to_close;
             if (left_to_close == 0u)
             {
                 return parse_main ();
             }

             continue;
         }

         * { continue; }
         $ { fprintf (stderr, "Error. Reached end of file while waiting for curly braces to close."); return KAN_FALSE;
         }
        */
    }
}

kan_bool_t parse_skip_until_enabled (void)
{
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         "//" separator* "\\c_interface_scanner_enable" separator* { return KAN_TRUE; }
         * { continue; }
         $
         {
             fprintf (stderr, "Error. Reached end of file while being disabled.");
             return KAN_FALSE;
         }
         */
    }
}

static kan_bool_t parse_subroutine_multi_line_comment (void)
{
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         !use:parse_meta;
         "*""/" { return KAN_TRUE; }
         * { continue; }
         $
         {
             fprintf (stderr, "Error. Reached end of file while waiting for multi line comment to close.");
             return KAN_FALSE;
         }
         */
    }
}

static kan_bool_t parse_subroutine_single_line_comment (void)
{
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         !use:parse_meta;
         "\n" { return KAN_TRUE; }
         * { continue; }
         $
         {
             fprintf (stderr, "Error. Reached end of file while waiting for multi line comment to close.");
             return KAN_FALSE;
         }
         */
    }
}

static kan_bool_t parse_input (void)
{
    optional_includable_object_begin ();
    kan_bool_t result = parse_main ();
    optional_includable_object_finish ();
    return result;
}

int main (int argument_count, char **arguments_array)
{
    if (argument_count != 4)
    {
        fprintf (stderr,
                 "Unknown arguments. Expected arguments:\n"
                 "- export_macro: macro used to mark exported functions and globals.\n"
                 "- input_file_path: path to input file.\n"
                 "- output_file_path: path to output file.\n");
        return RETURN_CODE_INVALID_ARGUMENTS;
    }

    arguments.export_macro = arguments_array[1u];
    arguments.input_file_path = arguments_array[2u];
    arguments.output_file_path = arguments_array[3u];

    kan_c_interface_file_init (&interface_file);
    interface_file.source_file_path = kan_allocate_general (kan_c_interface_allocation_group (),
                                                            strlen (arguments.input_file_path) + 1u, _Alignof (char));
    strcpy (interface_file.source_file_path, arguments.input_file_path);
    interface_file_should_have_includable_object = kan_c_interface_file_should_have_includable_object (&interface_file);

    io.input_stream = kan_direct_file_stream_open_for_read (arguments.input_file_path, KAN_FALSE);
    if (!io.input_stream)
    {
        fprintf (stderr, "Unable to open input file \"%s\".\n", arguments.input_file_path);
        return RETURN_CODE_UNABLE_TO_OPEN_INPUT;
    }

    if (!parse_input ())
    {
        fprintf (stderr, "Parse failed, exiting...\n");
        kan_direct_file_stream_close (io.input_stream);
        return RETURN_CODE_PARSE_FAILED;
    }

    kan_direct_file_stream_close (io.input_stream);
    interface_file.interface = kan_c_interface_build (reporting.first);

    if (!interface_file.interface)
    {
        fprintf (stderr, "Build failed, exiting...\n");
        return RETURN_CODE_BUILD_FAILED;
    }

    struct kan_stream_t *output_stream = kan_direct_file_stream_open_for_write (arguments.output_file_path, KAN_TRUE);
    if (!output_stream)
    {
        fprintf (stderr, "Unable to open output file \"%s\".\n", arguments.output_file_path);
        return RETURN_CODE_UNABLE_TO_OPEN_OUTPUT;
    }

    if (!kan_c_interface_file_serialize (&interface_file, output_stream))
    {
        fprintf (stderr, "Serialization failed, exiting...\n");
        kan_direct_file_stream_close (output_stream);
        return RETURN_CODE_SERIALIZATION_FAILED;
    }

    while (reporting.first)
    {
        struct kan_c_token_t *next = reporting.first->next;
        kan_free_batched (KAN_ALLOCATION_GROUP_IGNORE, reporting.first);
        reporting.first = next;
    }

    kan_direct_file_stream_close (output_stream);
    kan_c_interface_file_shutdown (&interface_file);
    return RETURN_CODE_SUCCESS;
}
