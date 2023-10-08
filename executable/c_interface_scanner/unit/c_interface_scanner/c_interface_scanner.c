#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <kan/api_common/bool.h>

#define RETURN_CODE_SUCCESS 0
#define RETURN_CODE_INVALID_ARGUMENTS (-1)
#define RETURN_CODE_UNABLE_TO_OPEN_INPUT (-2)
#define RETURN_CODE_PARSE_FAILED (-3)
#define RETURN_CODE_UNABLE_TO_OPEN_OUTPUT (-4)

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

static struct
{
    FILE *input_file;
    FILE *output_file;

    char input_file_buffer[INPUT_BUFFER_SIZE];
    char *limit;
    char *cursor;
    char *marker;
    char *token;

    kan_bool_t end_of_input_reached;
    size_t cursor_line;
    size_t cursor_symbol;
    size_t marker_line;
    size_t marker_symbol;
} io = {
    .input_file = NULL,
    .output_file = NULL,
    .input_file_buffer = {0},
    .limit = io.input_file_buffer + INPUT_BUFFER_SIZE - 1u,
    .cursor = io.input_file_buffer + INPUT_BUFFER_SIZE - 1u,
    .marker = io.input_file_buffer + INPUT_BUFFER_SIZE - 1u,
    .token = io.input_file_buffer + INPUT_BUFFER_SIZE - 1u,
    .end_of_input_reached = KAN_FALSE,
    .cursor_line = 1u,
    .cursor_symbol = 1u,
    .marker_line = 1u,
    .marker_symbol = 1u,
};

// IO

static int io_refill_buffer ()
{
    if (io.end_of_input_reached)
    {
        return INPUT_ERROR_REFIL_AFTER_END_OF_FILE;
    }

    const size_t shift = io.token - io.input_file_buffer;
    const size_t used = io.limit - io.token;

    if (shift < 1)
    {
        return INPUT_ERROR_LEXEME_OVERFLOW;
    }

    // Shift buffer contents (discard everything up to the current token).
    memmove (io.input_file_buffer, io.token, used);
    io.limit -= shift;
    io.cursor -= shift;
    io.marker -= shift;
    io.token -= shift;

    // Fill free space at the end of buffer with new data from file.
    io.limit += fread (io.limit, 1, INPUT_BUFFER_SIZE - used - 1u, io.input_file);
    io.limit[0u] = 0;
    io.end_of_input_reached = io.limit < io.input_file_buffer + INPUT_BUFFER_SIZE - 1u;

    return 0;
}

// Parse result reporting functions

static void report_meta_marker (const char *name_begin, const char *name_end)
{
    printf ("Found meta marker: ");
    while (name_begin != name_end)
    {
        printf ("%c", *name_begin);
        ++name_begin;
    }

    printf ("\n");
}

static void report_meta_integer (const char *name_begin,
                                 const char *name_end,
                                 const char *value_begin,
                                 const char *value_end)
{
    printf ("Found meta integer. Name: ");
    while (name_begin != name_end)
    {
        printf ("%c", *name_begin);
        ++name_begin;
    }

    printf (". Value: ");
    while (value_begin != value_end)
    {
        printf ("%c", *name_begin);
        ++value_begin;
    }

    printf ("\n");
}

static void report_meta_string (const char *name_begin,
                                const char *name_end,
                                const char *value_begin,
                                const char *value_end)
{
    printf ("Found meta string. Name: ");
    while (name_begin != name_end)
    {
        printf ("%c", *name_begin);
        ++name_begin;
    }

    printf (". Value: ");
    while (value_begin != value_end)
    {
        printf ("%c", *value_begin);
        ++value_begin;
    }

    printf ("\n");
}

enum archetype_t
{
    ARCHETYPE_BASIC,
    ARCHETYPE_STRUCT,
    ARCHETYPE_ENUM,
};

static void report_enum_begin (const char *name_begin, const char *name_end)
{
    printf ("Found enum: ");
    while (name_begin != name_end)
    {
        printf ("%c", *name_begin);
        ++name_begin;
    }

    printf ("\n");
}

static void report_enum_value (const char *name_begin, const char *name_end)
{
    printf ("    Found value. Name: ");
    while (name_begin != name_end)
    {
        printf ("%c", *name_begin);
        ++name_begin;
    }

    printf (".\n");
}

static void report_enum_end ()
{
    printf ("Enum end.\n");
}

static void report_struct_begin (const char *name_begin, const char *name_end)
{
    printf ("Found struct: ");
    while (name_begin != name_end)
    {
        printf ("%c", *name_begin);
        ++name_begin;
    }

    printf ("\n");
}

static void report_struct_field (kan_bool_t is_const,
                                 enum archetype_t archetype,
                                 const char *type_name_begin,
                                 const char *type_name_end,
                                 size_t pointer_level,
                                 const char *name_begin,
                                 const char *name_end,
                                 kan_bool_t is_array)
{
    printf ("    Found field. Const: %s. Archetype: %s. Array: %s. Pointer level: %lld. Type: ", is_const ? "1" : "0",
            archetype == ARCHETYPE_BASIC  ? "basic" :
            archetype == ARCHETYPE_STRUCT ? "struct" :
                                            "enum",
            is_array ? "1" : "0", pointer_level);

    while (type_name_begin != type_name_end)
    {
        printf ("%c", *type_name_begin);
        ++type_name_begin;
    }

    printf (". Name: ");
    while (name_begin != name_end)
    {
        printf ("%c", *name_begin);
        ++name_begin;
    }

    printf (".\n");
}

static void report_struct_end ()
{
    printf ("Struct end.\n");
}

static void report_exported_function_begin (kan_bool_t is_return_const,
                                            enum archetype_t return_archetype,
                                            const char *type_name_begin,
                                            const char *type_name_end,
                                            size_t pointer_level,
                                            const char *name_begin,
                                            const char *name_end)
{
    printf ("Function start. Return const: %s. Return archetype: %s. Return type name: ", is_return_const ? "1" : "0",
            return_archetype == ARCHETYPE_BASIC  ? "basic" :
            return_archetype == ARCHETYPE_STRUCT ? "struct" :
                                                   "enum");

    while (type_name_begin != type_name_end)
    {
        printf ("%c", *type_name_begin);
        ++type_name_begin;
    }

    printf (". Pointer level: %lld. Name: ", pointer_level);
    while (name_begin != name_end)
    {
        printf ("%c", *name_begin);
        ++name_begin;
    }

    printf (".\n");
}

static void report_exported_function_argument (kan_bool_t is_const,
                                               enum archetype_t archetype,
                                               const char *type_name_begin,
                                               const char *type_name_end,
                                               size_t pointer_level,
                                               const char *name_begin,
                                               const char *name_end)
{
    printf ("    Found field. Const: %s. Archetype: %s. Pointer level: %lld. Type: ", is_const ? "1" : "0",
            archetype == ARCHETYPE_BASIC  ? "basic" :
            archetype == ARCHETYPE_STRUCT ? "struct" :
                                            "enum",
            pointer_level);

    while (type_name_begin != type_name_end)
    {
        printf ("%c", *type_name_begin);
        ++type_name_begin;
    }

    printf (". Name: ");
    while (name_begin != name_end)
    {
        printf ("%c", *name_begin);
        ++name_begin;
    }

    printf (".\n");
}

static void report_exported_function_end ()
{
    printf ("Function end.\n");
}

static void report_exported_symbol (kan_bool_t is_const,
                                    enum archetype_t archetype,
                                    const char *type_name_begin,
                                    const char *type_name_end,
                                    size_t pointer_level,
                                    const char *name_begin,
                                    const char *name_end,
                                    kan_bool_t is_array)
{
    printf (
        "Found exported symbol. Const: %s. Archetype: %s. Array: %s. Pointer level: %lld. Type: ", is_const ? "1" : "0",
        archetype == ARCHETYPE_BASIC  ? "basic" :
        archetype == ARCHETYPE_STRUCT ? "struct" :
                                        "enum",
        is_array ? "1" : "0", pointer_level);

    while (type_name_begin != type_name_end)
    {
        printf ("%c", *type_name_begin);
        ++type_name_begin;
    }

    printf (". Name: ");
    while (name_begin != name_end)
    {
        printf ("%c", *name_begin);
        ++name_begin;
    }

    printf (".\n");
}

// Parse input using re2c

// Define tag format.
/*!stags:re2c format = 'const char *@@ = NULL;';*/

// Helpers for re2c api.
static void re2c_yyskip ()
{
    if (*io.cursor == '\n')
    {
        ++io.cursor_line;
        io.cursor_symbol = 0u;
    }

    ++io.cursor;
    ++io.cursor_symbol;
}

static void re2c_yybackup ()
{
    io.marker = io.cursor;
    io.marker_line = io.cursor_line;
    io.marker_symbol = io.cursor_symbol;
}

static void re2c_yyrestore ()
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
 "/""*" { if (!parse_subroutine_multi_line_comment ()) { return KAN_FALSE; } continue; }
 "//" { if (!parse_subroutine_single_line_comment ()) { return KAN_FALSE; } continue; }

 separator | any_preprocessor { continue; }
 *
 {
     fprintf (stderr, "Error. [%lld:%lld]: Unable to parse next token. Parser: %s. Symbol code: 0x%x.\n",
         io.cursor_line, io.cursor_symbol, __func__, (int) *io.cursor);
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

static kan_bool_t parse_main ();
static kan_bool_t parse_enum ();
static kan_bool_t parse_struct ();
static kan_bool_t parse_exported_symbol_begin ();
static kan_bool_t parse_exported_function_arguments ();
static kan_bool_t parse_skip_until_round_braces_close ();
static kan_bool_t parse_skip_until_curly_braces_close ();

static kan_bool_t parse_subroutine_multi_line_comment ();
static kan_bool_t parse_subroutine_single_line_comment ();

static kan_bool_t parse_main ()
{
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         !use:default;

         identifier (separator | [;])
         {
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
            report_enum_begin (capture_identifier_begin, capture_identifier_end);
            return parse_enum ();
        }

         // Named structure.
         "struct" separator+ identifier separator* "{"
         {
             report_struct_begin (capture_identifier_begin, capture_identifier_end);
             return parse_struct ();
         }

         // Forbidden typedef.
         "typedef" separator+ ("enum" | "struct") [^;]+ ";"
         {
             fprintf (stderr, "Error. [%lld:%lld]: Encountered struct/enum hiding typedef, it confuses parser.\n",
                io.cursor_line, io.cursor_symbol);
             return KAN_FALSE;
         }

         // We ignore usual typedefs.
         "typedef" separator+ [^;]+ ";" { continue; }

         // Looks like we've encountered '(' from function that is not exported. Skip everything inside.
         "(" { return parse_skip_until_round_braces_close (); }

         // Looks like we've encountered '{' from function body or initializer. Skip everything inside.
         "{" { return parse_skip_until_curly_braces_close (); }

         // Symbols about which we're not concerned while they're in global scope.
         [*=;] { continue; }

         $ { return KAN_TRUE; }
         */
    }
}

static kan_bool_t parse_enum ()
{
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         !use:default;

         identifier (separator? "=" separator* [0-9a-zA-Z_]+ separator*)?
         {
             report_enum_value (
                 capture_identifier_begin,
                 capture_identifier_end);
             continue;
         }

         "," { continue; }

         "}" { report_enum_end (); return parse_main (); }

         $ { fprintf (stderr, "Error. Reached end of file while parsing enum."); return KAN_FALSE; }
         */
    }
}

static kan_bool_t parse_struct ()
{
    kan_bool_t inside_union = KAN_FALSE;
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         !use:default;

         type separator* identifier separator* array_suffix? separator* ";"
         {
             report_struct_field (
                 capture_const_begin == capture_const_end ? KAN_FALSE : KAN_TRUE,
                 capture_enum_begin != capture_enum_end     ? ARCHETYPE_ENUM :
                 capture_struct_begin != capture_struct_end ? ARCHETYPE_STRUCT :
                                                              ARCHETYPE_BASIC,
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
             if (inside_union)
             {
                 fprintf (stderr, "Error. [%lld:%lld]: Nested unions aren't supported.\n",
                     io.cursor_line, io.cursor_symbol);
                 return KAN_FALSE;
             }

             inside_union = KAN_TRUE;
             continue;
         }

         ";" { continue; }

         "}"
         {
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

static kan_bool_t parse_exported_symbol_begin ()
{
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         !use:default;

         // Exported function
         type separator* identifier separator* "("
         {
             report_exported_function_begin (
                 capture_const_begin == capture_const_end ? KAN_FALSE : KAN_TRUE,
                 capture_enum_begin != capture_enum_end     ? ARCHETYPE_ENUM :
                 capture_struct_begin != capture_struct_end ? ARCHETYPE_STRUCT :
                                                              ARCHETYPE_BASIC,
                 capture_type_name_begin,
                 capture_type_name_end,
                 capture_pointer_end - capture_pointer_begin,
                 capture_identifier_begin,
                 capture_identifier_end);
             return parse_exported_function_arguments ();
         }

         // Exported symbol
         type separator* identifier separator* array_suffix? separator* (";" | "=")
         {
             report_exported_symbol (
                 capture_const_begin == capture_const_end ? KAN_FALSE : KAN_TRUE,
                 capture_enum_begin != capture_enum_end     ? ARCHETYPE_ENUM :
                 capture_struct_begin != capture_struct_end ? ARCHETYPE_STRUCT :
                                                              ARCHETYPE_BASIC,
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

static kan_bool_t parse_exported_function_arguments ()
{
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         !use:default;

         type separator* identifier separator*
         {
            report_exported_function_argument (
                capture_const_begin == capture_const_end ? KAN_FALSE : KAN_TRUE,
                capture_enum_begin != capture_enum_end     ? ARCHETYPE_ENUM :
                capture_struct_begin != capture_struct_end ? ARCHETYPE_STRUCT :
                                                             ARCHETYPE_BASIC,
                capture_type_name_begin,
                capture_type_name_end,
                capture_pointer_end - capture_pointer_begin,
                capture_identifier_begin,
                capture_identifier_end);
            continue;
         }

         "," { continue; }

         ")"
         {
             report_exported_function_end ();
             return parse_main ();
         }

         $ { fprintf (stderr, "Error. Reached end of file while parsing function arguments."); return KAN_FALSE; }
         */
    }
}

static kan_bool_t parse_skip_until_round_braces_close ()
{
    size_t left_to_close = 1u;
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         !use:skip_comments;

         "("
         {
             ++left_to_close;
             continue;
         }

         ")"
         {
             --left_to_close;
             if (left_to_close == 0u)
             {
                 return parse_main ();
             }

             continue;
         }

         * { continue; }
         $ { fprintf (stderr, "Error. Reached end of file while waiting for round braces to close."); return KAN_FALSE;
         }
        */
    }
}

static kan_bool_t parse_skip_until_curly_braces_close ()
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

static kan_bool_t parse_subroutine_multi_line_comment ()
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
             fprintf (stderr, "Error. Reached end of file while waiting for multi line comment to close ");
             return KAN_FALSE;
         }
         */
    }
}

static kan_bool_t parse_subroutine_single_line_comment ()
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
             fprintf (stderr, "Error. Reached end of file while waiting for multi line comment to close ");
             return KAN_FALSE;
         }
         */
    }
}

static kan_bool_t parse_input ()
{
    return parse_main ();
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

    fprintf (stdout,
             "Running with arguments:\n"
             "- export_macro: \"%s\"\n"
             "- input_file_path: \"%s\"\n"
             "- output_file_path: \"%s\"\n",
             arguments.export_macro, arguments.input_file_path, arguments.output_file_path);

    io.input_file = fopen (arguments.input_file_path, "r");
    if (!io.input_file)
    {
        fprintf (stderr, "Unable to open input file \"%s\".\n", arguments.input_file_path);
        return RETURN_CODE_UNABLE_TO_OPEN_INPUT;
    }

    if (!parse_input ())
    {
        fprintf (stderr, "Parse failed, exiting...\n");
        fclose (io.input_file);
        return RETURN_CODE_PARSE_FAILED;
    }

    io.output_file = fopen (arguments.output_file_path, "w");
    if (!io.output_file)
    {
        fprintf (stderr, "Unable to open output file \"%s\".\n", arguments.output_file_path);
        return RETURN_CODE_UNABLE_TO_OPEN_OUTPUT;
    }

    fclose (io.input_file);
    fclose (io.output_file);
    return RETURN_CODE_SUCCESS;
}
