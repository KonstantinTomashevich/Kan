#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <kan/api_common/mute_warnings.h>
#include <kan/container/hash_storage.h>
#include <kan/container/interned_string.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/container/trivial_string_buffer.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/path_container.h>
#include <kan/file_system/stream.h>
#include <kan/memory/allocation.h>
#include <kan/stream/random_access_stream_buffer.h>

#define RETURN_CODE_SUCCESS 0
#define RETURN_CODE_INVALID_ARGUMENTS (-1)
#define RETURN_CODE_TARGET_LIST_LOAD_FAILED (-2)
#define RETURN_CODE_INPUT_LIST_LOAD_FAILED (-3)
#define RETURN_CODE_PARSE_FAILED (-4)
#define RETURN_CODE_WRITE_FAILED (-5)

enum target_file_type_t
{
    TARGET_FILE_TYPE_HEADER,
    TARGET_FILE_TYPE_OBJECT,
    TARGET_FILE_TYPE_UNKNOWN,
};

struct target_file_node_t
{
    struct kan_hash_storage_node_t node;
    enum target_file_type_t type;
    char *path;
    kan_instance_size_t found_in_input_index;
};

struct included_file_node_t
{
    struct kan_hash_storage_node_t node;
    char *path;
};

static struct
{
    const char *product;
    const char *unit_name;
    const char *target_file_list;
    const char *input_file_list;
} arguments;

static struct
{
    struct kan_trivial_string_buffer_t declaration_section;
    struct kan_trivial_string_buffer_t generation_control_section;
    struct kan_trivial_string_buffer_t generated_functions_section;
    struct kan_trivial_string_buffer_t generated_symbols_section;
    struct kan_trivial_string_buffer_t bootstrap_section;
    struct kan_trivial_string_buffer_t registrar_section;

    struct kan_stack_group_allocator_t persistent_allocator;
    struct kan_hash_storage_t target_files;
    struct kan_hash_storage_t included_files;

    kan_instance_size_t current_input_index;

    kan_allocation_group_t main_allocation_group;
    kan_allocation_group_t section_allocation_group;
    kan_allocation_group_t persistent_allocation_group;
    kan_allocation_group_t hash_storage_allocation_group;
    kan_allocation_group_t meta_allocation_group;
} global;

static struct
{
    kan_interned_string_t type_void;
    kan_interned_string_t type_char;
    kan_interned_string_t type_interned_string;
    kan_interned_string_t type_dynamic_array;
    kan_interned_string_t type_patch;
} interned;

struct re2c_tags_t
{
    /*!stags:re2c format = 'const char *@@;';*/
};

enum type_info_group_t
{
    TYPE_INFO_GROUP_VALUE = 0u,
    TYPE_INFO_GROUP_ENUM,
    TYPE_INFO_GROUP_STRUCT,
};

struct type_info_t
{
    kan_interned_string_t name;
    enum type_info_group_t group;
    kan_bool_t is_const;
    uint8_t pointer_level;
};

struct visibility_condition_value_node_t
{
    struct visibility_condition_value_node_t *next;
    kan_interned_string_t value;
};

struct top_level_meta_t
{
    struct top_level_meta_t *next;
    kan_interned_string_t top_level_name;
};

struct secondary_level_meta_t
{
    struct secondary_level_meta_t *next;
    kan_interned_string_t top_level_name;
    kan_interned_string_t secondary_level_name;
};

struct meta_storage_t
{
    kan_bool_t export;
    kan_bool_t flags;
    kan_bool_t ignore;
    kan_bool_t external_pointer;
    kan_bool_t has_dynamic_array_type;

    kan_interned_string_t explicit_init_functor;
    kan_interned_string_t explicit_shutdown_functor;

    struct type_info_t dynamic_array_type;
    kan_interned_string_t size_field;
    kan_interned_string_t visibility_condition_field;
    struct visibility_condition_value_node_t *first_visibility_condition_value;

    struct top_level_meta_t *first_enum_meta;
    struct secondary_level_meta_t *first_enum_value_meta;

    struct top_level_meta_t *first_struct_meta;
    struct secondary_level_meta_t *first_struct_field_meta;

    struct top_level_meta_t *first_function_meta;
    struct secondary_level_meta_t *first_function_argument_meta;
};

static struct
{
    struct kan_stream_t *stream;

    char *limit;
    const char *cursor;
    const char *marker;
    const char *token;

    kan_bool_t end_of_input_reached;
    size_t cursor_line;
    size_t cursor_symbol;
    size_t marker_line;
    size_t marker_symbol;

    struct target_file_node_t *current_target_node;
    size_t current_target_line;

    struct meta_storage_t current_meta_storage;

    const char *saved;
    size_t saved_line;
    size_t saved_symbol;
    size_t saved_current_file_line;

    struct re2c_tags_t tags;

    char input_buffer[KAN_REFLECTION_PREPROCESSOR_IO_BUFFER];

} parser = {
    .stream = NULL,
    .limit = parser.input_buffer + KAN_REFLECTION_PREPROCESSOR_IO_BUFFER - 1u,
    .cursor = parser.input_buffer + KAN_REFLECTION_PREPROCESSOR_IO_BUFFER - 1u,
    .marker = parser.input_buffer + KAN_REFLECTION_PREPROCESSOR_IO_BUFFER - 1u,
    .token = parser.input_buffer + KAN_REFLECTION_PREPROCESSOR_IO_BUFFER - 1u,
    .end_of_input_reached = KAN_FALSE,
};

enum parse_status_t
{
    PARSE_STATUS_IN_PROGRESS = 0u,
    PARSE_STATUS_FINISHED,
    PARSE_STATUS_FAILED,
};

static struct target_file_node_t *create_target_file_node (const char *file)
{
    struct target_file_node_t *node =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&global.persistent_allocator, struct target_file_node_t);
    node->node.hash = kan_string_hash (file);

    const kan_instance_size_t length = (kan_instance_size_t) strlen (file);
    node->path = kan_stack_group_allocator_allocate (&global.persistent_allocator, length + 1u, _Alignof (char));
    memcpy (node->path, file, length + 1u);
    node->type = TARGET_FILE_TYPE_UNKNOWN;

    if (length > 2u && file[length - 2u] == '.' && file[length - 1u] == 'h')
    {
        node->type = TARGET_FILE_TYPE_HEADER;
    }
    else if (length > 2u && file[length - 2u] == '.' && file[length - 1u] == 'c')
    {
        node->type = TARGET_FILE_TYPE_OBJECT;
    }

    node->found_in_input_index = KAN_INT_MAX (kan_instance_size_t);
    kan_hash_storage_update_bucket_count_default (&global.target_files,
                                                  KAN_REFLECTION_PREPROCESSOR_TARGET_FILE_BUCKETS);
    kan_hash_storage_add (&global.target_files, &node->node);
    return node;
}

static struct target_file_node_t *find_target_file_node (const char *file_name_begin, const char *file_name_end)
{
    // Path might be fairly inconsistent for some preprocessors like MSVC preprocessor that somehow mixes both Unix and
    // Win32 path conventions without any perceivable distinction. Therefore, we manually convert paths to Unix format.
    struct kan_file_system_path_container_t path_container;
    path_container.length = 0u;

    while (file_name_begin != file_name_end)
    {
        KAN_ASSERT (path_container.length < KAN_FILE_SYSTEM_MAX_PATH_LENGTH)
        if (*file_name_begin == '\\' && file_name_begin + 1u != file_name_end && *(file_name_begin + 1u) == '\\')
        {
            // Replace Win32 path separator with Unix.
            path_container.path[path_container.length] = '/';
            ++path_container.length;
            ++file_name_begin;
        }
        else
        {
            path_container.path[path_container.length] = *file_name_begin;
            ++path_container.length;
        }

        ++file_name_begin;
    }

    path_container.path[path_container.length] = '\0';
    const kan_hash_t file_name_hash =
        kan_char_sequence_hash (path_container.path, path_container.path + path_container.length);

    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (&global.target_files, file_name_hash);
    struct target_file_node_t *node = (struct target_file_node_t *) bucket->first;
    const struct target_file_node_t *node_end =
        (struct target_file_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->node.hash == file_name_hash)
        {
            const kan_instance_size_t node_length = (kan_instance_size_t) strlen (node->path);
            if (node_length == path_container.length && memcmp (path_container.path, node->path, node_length) == 0)
            {
                return node;
            }
        }

        node = (struct target_file_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static void include_file (const char *file_name_begin, const char *file_name_end)
{
    const kan_hash_t file_name_hash = kan_char_sequence_hash (file_name_begin, file_name_end);
    const kan_instance_size_t file_name_length = (kan_instance_size_t) (file_name_end - file_name_begin);

    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (&global.included_files, file_name_hash);
    struct included_file_node_t *node = (struct included_file_node_t *) bucket->first;
    const struct included_file_node_t *node_end =
        (struct included_file_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->node.hash == file_name_hash)
        {
            const kan_instance_size_t node_length = (kan_instance_size_t) strlen (node->path);
            if (node_length == file_name_length && memcmp (file_name_begin, node->path, node_length) == 0)
            {
                return;
            }
        }

        node = (struct included_file_node_t *) node->node.list_node.next;
    }

    node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&global.persistent_allocator, struct included_file_node_t);
    node->node.hash = file_name_hash;

    node->path =
        kan_stack_group_allocator_allocate (&global.persistent_allocator, file_name_length + 1u, _Alignof (char));
    memcpy (node->path, file_name_begin, file_name_length);
    node->path[file_name_length] = '\0';

    kan_hash_storage_update_bucket_count_default (&global.included_files,
                                                  KAN_REFLECTION_PREPROCESSOR_INCLUDED_FILE_BUCKETS);
    kan_hash_storage_add (&global.included_files, &node->node);

    // Check file existence (and check that it is file) to defend from builtins and strange GCC directory includes.
    struct kan_file_system_entry_status_t status;

    if (kan_file_system_query_entry (node->path, &status) && status.type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE)
    {
        kan_trivial_string_buffer_append_string (&global.declaration_section, "#include \"");
        kan_trivial_string_buffer_append_char_sequence (&global.declaration_section, file_name_begin, file_name_length);
        kan_trivial_string_buffer_append_string (&global.declaration_section, "\"\n");
    }
}

static void meta_storage_init (struct meta_storage_t *storage)
{
    storage->export = KAN_FALSE;
    storage->flags = KAN_FALSE;
    storage->ignore = KAN_FALSE;
    storage->external_pointer = KAN_FALSE;
    storage->has_dynamic_array_type = KAN_FALSE;

    storage->explicit_init_functor = NULL;
    storage->explicit_shutdown_functor = NULL;

    storage->size_field = NULL;
    storage->visibility_condition_field = NULL;
    storage->first_visibility_condition_value = NULL;

    storage->first_enum_meta = NULL;
    storage->first_enum_value_meta = NULL;

    storage->first_struct_meta = NULL;
    storage->first_struct_field_meta = NULL;

    storage->first_function_meta = NULL;
    storage->first_function_argument_meta = NULL;
}

static kan_bool_t meta_storage_is_empty (struct meta_storage_t *storage)
{
    return !storage->export && !storage->flags && !storage->ignore && !storage->external_pointer &&
           !storage->has_dynamic_array_type && !storage->size_field && !storage->visibility_condition_field &&
           !storage->first_visibility_condition_value && !storage->first_enum_meta && !storage->first_enum_value_meta &&
           !storage->first_struct_meta && !storage->first_struct_field_meta && !storage->first_function_meta &&
           !storage->first_function_argument_meta;
}

static void meta_storage_shutdown (struct meta_storage_t *storage)
{
    struct visibility_condition_value_node_t *value_node = storage->first_visibility_condition_value;
    while (value_node)
    {
        struct visibility_condition_value_node_t *next = value_node->next;
        kan_free_batched (global.meta_allocation_group, value_node);
        value_node = next;
    }

    struct top_level_meta_t *top_level_node = storage->first_enum_meta;
    while (top_level_node)
    {
        struct top_level_meta_t *next = top_level_node->next;
        kan_free_batched (global.meta_allocation_group, top_level_node);
        top_level_node = next;
    }

    struct secondary_level_meta_t *secondary_level_node = storage->first_enum_value_meta;
    while (secondary_level_node)
    {
        struct secondary_level_meta_t *next = secondary_level_node->next;
        kan_free_batched (global.meta_allocation_group, secondary_level_node);
        secondary_level_node = next;
    }

    top_level_node = storage->first_struct_meta;
    while (top_level_node)
    {
        struct top_level_meta_t *next = top_level_node->next;
        kan_free_batched (global.meta_allocation_group, top_level_node);
        top_level_node = next;
    }

    secondary_level_node = storage->first_struct_field_meta;
    while (secondary_level_node)
    {
        struct secondary_level_meta_t *next = secondary_level_node->next;
        kan_free_batched (global.meta_allocation_group, secondary_level_node);
        secondary_level_node = next;
    }

    top_level_node = storage->first_function_meta;
    while (top_level_node)
    {
        struct top_level_meta_t *next = top_level_node->next;
        kan_free_batched (global.meta_allocation_group, top_level_node);
        top_level_node = next;
    }

    secondary_level_node = storage->first_function_argument_meta;
    while (secondary_level_node)
    {
        struct secondary_level_meta_t *next = secondary_level_node->next;
        kan_free_batched (global.meta_allocation_group, secondary_level_node);
        secondary_level_node = next;
    }
}

static void reset_parser_state (struct kan_stream_t *new_stream)
{
    parser.stream = new_stream;
    parser.limit = parser.input_buffer + KAN_REFLECTION_PREPROCESSOR_IO_BUFFER - 1u;
    parser.cursor = parser.input_buffer + KAN_REFLECTION_PREPROCESSOR_IO_BUFFER - 1u;
    parser.marker = parser.input_buffer + KAN_REFLECTION_PREPROCESSOR_IO_BUFFER - 1u;
    parser.token = parser.input_buffer + KAN_REFLECTION_PREPROCESSOR_IO_BUFFER - 1u;
    *parser.limit = '\0';

    parser.end_of_input_reached = KAN_FALSE;
    parser.cursor_line = 1u;
    parser.cursor_symbol = 1u;
    parser.marker_line = 1u;
    parser.marker_symbol = 1u;

    parser.current_target_node = NULL;
    parser.current_target_line = 1u;

    parser.saved = parser.cursor;
    parser.saved_line = parser.cursor_line;
    parser.saved_symbol = parser.cursor_symbol;
    parser.saved_current_file_line = parser.current_target_line;

    meta_storage_shutdown (&parser.current_meta_storage);
    meta_storage_init (&parser.current_meta_storage);
}

static void parser_enter_file (const char *file_name_begin,
                               const char *file_name_end,
                               const char *line_begin,
                               const char *line_end)
{
    struct target_file_node_t *new_file = find_target_file_node (file_name_begin, file_name_end);
    struct target_file_node_t *previous_file = parser.current_target_node;

    if (new_file && !previous_file && new_file->type == TARGET_FILE_TYPE_HEADER)
    {
        include_file (file_name_begin, file_name_end);
    }

    if (previous_file && !new_file && previous_file->type == TARGET_FILE_TYPE_OBJECT)
    {
        include_file (file_name_begin, file_name_end);
    }

    if (new_file && new_file->found_in_input_index == KAN_INT_MAX (kan_instance_size_t))
    {
        new_file->found_in_input_index = global.current_input_index;
    }

    parser.current_target_node = new_file;
    if (new_file)
    {
        parser.current_target_line = 0u;
        while (line_begin < line_end)
        {
            KAN_ASSERT (*line_begin >= '0' && *line_begin <= '9')
            parser.current_target_line = parser.current_target_line * 10u + *line_begin - '0';
            ++line_begin;
        }

        --parser.current_target_line;
    }
    else
    {
        parser.current_target_line = 1u;
    }
}

static int re2c_refill_buffer (void)
{
#define ERROR_REFILL_AFTER_END_OF_FILE -1
#define ERROR_LEXEME_OVERFLOW -2

    if (parser.end_of_input_reached)
    {
        return ERROR_REFILL_AFTER_END_OF_FILE;
    }

    const size_t shift = parser.token - parser.input_buffer;
    const size_t used = parser.limit - parser.token;

    if (shift < 1)
    {
        return ERROR_LEXEME_OVERFLOW;
    }

    // Shift buffer contents (discard everything up to the current token).
    memmove (parser.input_buffer, parser.token, used);
    parser.limit -= shift;
    parser.cursor -= shift;
    parser.marker -= shift;
    parser.token -= shift;

    const char **first_tag = (const char **) &parser.tags;
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
    parser.limit += parser.stream->operations->read (parser.stream, KAN_REFLECTION_PREPROCESSOR_IO_BUFFER - used - 1u,
                                                     parser.limit);
    parser.limit[0u] = 0;
    parser.end_of_input_reached = parser.limit < parser.input_buffer + KAN_REFLECTION_PREPROCESSOR_IO_BUFFER - 1u;
    return 0;

#undef ERROR_REFILL_AFTER_END_OF_FILE
#undef ERROR_LEXEME_OVERFLOW
}

static inline void re2c_yyskip (void)
{
    if (*parser.cursor == '\n')
    {
        ++parser.cursor_line;
        ++parser.current_target_line;
        parser.cursor_symbol = 0u;
    }

    ++parser.cursor;
    ++parser.cursor_symbol;
}

static inline void re2c_yybackup (void)
{
    parser.marker = parser.cursor;
    parser.marker_line = parser.cursor_line;
    parser.marker_symbol = parser.cursor_symbol;
}

static inline void re2c_yyrestore (void)
{
    parser.cursor = parser.marker;
    parser.cursor_line = parser.marker_line;
    parser.cursor_symbol = parser.marker_symbol;
}

/*!re2c
 re2c:api = custom;
 re2c:api:style = free-form;
 re2c:define:YYCTYPE  = char;
 re2c:define:YYLESSTHAN = "parser.cursor >= parser.limit";
 re2c:define:YYPEEK = "*parser.cursor";
 re2c:define:YYSKIP = "re2c_yyskip ();";
 re2c:define:YYBACKUP = "re2c_yybackup ();";
 re2c:define:YYRESTORE = "re2c_yyrestore ();";
 re2c:define:YYFILL   = "re2c_refill_buffer () == 0";
 re2c:define:YYSTAGP = "@@{tag} = parser.cursor;";
 re2c:define:YYSTAGN = "@@{tag} = NULL;";
 re2c:define:YYSHIFTSTAG  = "@@{tag} += @@{shift};";
 re2c:eof = 0;
 re2c:tags = 1;
 re2c:tags:expression = "parser.tags.@@";

 separator = [\x20\x0c\x0a\x0d\x09\x0b];
 separator_no_nl = [\x20\x0c\x0d\x09\x0b];
 separators_till_nl = [\x20\x0c\x0d\x09\x0b]* [\x0a];
 identifier = [A-Za-z_][A-Za-z0-9_]*;

 type_prefix =
     (@const_marker "const" separator+)? ((@struct_marker "struct" separator+) | (@enum_marker "enum" separator+))?;
 type_pointer_suffix = (@pointer_begin separator* "*"+ separator* @pointer_end);
 type = type_prefix @type_name_begin identifier @type_name_end (type_pointer_suffix | (separator+));

 array_suffix = "[" @array_size_begin ((. \ [\]]) | separator)* @array_size_end "]";

 line_pragma = "line"? separator_no_nl+ @line_begin [0-9]+ @line_end separator_no_nl+
     "\"" @file_begin (. \ [\x22])+ @file_end "\"" ((separator_no_nl+ [1-4])+)? separators_till_nl;
 */

#define TYPE_MARKERS                                                                                                   \
    const char *const_marker;                                                                                          \
    const char *struct_marker;                                                                                         \
    const char *enum_marker;                                                                                           \
    const char *type_name_begin;                                                                                       \
    const char *type_name_end;                                                                                         \
    const char *pointer_begin;                                                                                         \
    const char *pointer_end

#define ASSEMBLE_TYPE_INFO(OUTPUT)                                                                                     \
    (OUTPUT).name = kan_char_sequence_intern (type_name_begin, type_name_end);                                         \
    (OUTPUT).group =                                                                                                   \
        struct_marker ? TYPE_INFO_GROUP_STRUCT : (enum_marker ? TYPE_INFO_GROUP_ENUM : TYPE_INFO_GROUP_VALUE);         \
    (OUTPUT).is_const = const_marker != NULL;                                                                          \
    (OUTPUT).pointer_level = 0u;                                                                                       \
                                                                                                                       \
    while (pointer_begin != pointer_end)                                                                               \
    {                                                                                                                  \
        if (*pointer_begin == '*')                                                                                     \
        {                                                                                                              \
            ++(OUTPUT).pointer_level;                                                                                  \
        }                                                                                                              \
                                                                                                                       \
        ++pointer_begin;                                                                                               \
    }

static inline enum parse_status_t continue_into_potential_pragma (kan_bool_t allow_file_switch)
{
    const char *line_begin;
    const char *line_end;
    const char *file_begin;
    const char *file_end;
    TYPE_MARKERS;
    const char *name_begin;
    const char *name_end;
    const char *value_begin;
    const char *value_end;
    parser.token = parser.cursor;

    // Because GCC family uses #pragma and MSVC family uses __pragma(),
    // we need a fairly strange regular expressions in this function.

    /*!re2c
     line_pragma
     {
         struct target_file_node_t *old_node = parser.current_target_node;
         size_t old_file_line = parser.current_target_line - 1u;
         parser_enter_file (file_begin, file_end, line_begin, line_end);

         if (old_node != parser.current_target_node)
         {
             if (allow_file_switch && !meta_storage_is_empty (&parser.current_meta_storage))
             {
                 fprintf (stderr, "[%s:%lu:%lu] Encountered file switch with dangling meta, which is not allowed.\n",
                     old_node ? old_node->path : "<unknown>", (unsigned long) old_file_line, (unsigned long)
                     parser.cursor_symbol - 1u); return PARSE_STATUS_FAILED;
                 return PARSE_STATUS_FAILED;
             }

             if (!allow_file_switch)
             {
                 fprintf (stderr, "[%s:%lu:%lu] Encountered file switch in context that does not support it.\n",
                     old_node ? old_node->path : "<unknown>", (unsigned long) old_file_line, (unsigned long)
                     parser.cursor_symbol - 1u); return PARSE_STATUS_FAILED;
                 return PARSE_STATUS_FAILED;
             }
         }

         return PARSE_STATUS_IN_PROGRESS;
     }

     ("pragma" separator_no_nl+)? "kan_export" (")" | separators_till_nl)
     {
         if (parser.current_meta_storage.export)
         {
             fprintf (stderr, "[%s:%lu:%lu] Encountered duplicate export meta.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line - 1u, (unsigned long) parser.cursor_symbol - 1u);
             return PARSE_STATUS_FAILED;
         }

         parser.current_meta_storage.export = KAN_TRUE;
         return PARSE_STATUS_IN_PROGRESS;
     }

     ("pragma" separator_no_nl+)? "kan_reflection_flags" (")" | separators_till_nl)
     {
         if (parser.current_meta_storage.flags)
         {
             fprintf (stderr, "[%s:%lu:%lu] Encountered duplicate flags meta.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line - 1u, (unsigned long) parser.cursor_symbol - 1u);
             return PARSE_STATUS_FAILED;
         }

         parser.current_meta_storage.flags = KAN_TRUE;
         return PARSE_STATUS_IN_PROGRESS;
     }

     ("pragma" separator_no_nl+)? "kan_reflection_ignore" (")" | separators_till_nl)
     {
         if (parser.current_meta_storage.ignore)
         {
             fprintf (stderr, "[%s:%lu:%lu] Encountered duplicate ignore meta.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line - 1u, (unsigned long) parser.cursor_symbol - 1u);
             return PARSE_STATUS_FAILED;
         }

         parser.current_meta_storage.ignore = KAN_TRUE;
         return PARSE_STATUS_IN_PROGRESS;
     }

     ("pragma" separator_no_nl+)? "kan_reflection_external_pointer" (")" | separators_till_nl)
     {
         if (parser.current_meta_storage.external_pointer)
         {
             fprintf (stderr, "[%s:%lu:%lu] Encountered duplicate external pointer meta.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line - 1u, (unsigned long) parser.cursor_symbol - 1u);
             return PARSE_STATUS_FAILED;
         }

         parser.current_meta_storage.external_pointer = KAN_TRUE;
         return PARSE_STATUS_IN_PROGRESS;
     }

     ("pragma" separator_no_nl+)? "kan_reflection_explicit_init_functor" separator_no_nl+
     @name_begin identifier @name_end ")"?  separators_till_nl
     {
         if (parser.current_meta_storage.explicit_init_functor)
         {
             fprintf (stderr, "[%s:%lu:%lu] Encountered duplicate explicit init functor meta.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line - 1u, (unsigned long) parser.cursor_symbol - 1u);
             return PARSE_STATUS_FAILED;
         }

         parser.current_meta_storage.explicit_init_functor = kan_char_sequence_intern (name_begin, name_end);
         return PARSE_STATUS_IN_PROGRESS;
     }

     ("pragma" separator_no_nl+)? "kan_reflection_explicit_shutdown_functor" separator_no_nl+
     @name_begin identifier @name_end (")" | separators_till_nl)
     {
         if (parser.current_meta_storage.explicit_shutdown_functor)
         {
             fprintf (stderr, "[%s:%lu:%lu] Encountered duplicate explicit shutdown functor meta.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line - 1u, (unsigned long) parser.cursor_symbol - 1u);
             return PARSE_STATUS_FAILED;
         }

         parser.current_meta_storage.explicit_shutdown_functor = kan_char_sequence_intern (name_begin, name_end);
         return PARSE_STATUS_IN_PROGRESS;
     }

     ("pragma" separator_no_nl+)? "kan_reflection_dynamic_array_type" separator_no_nl+
     type_prefix @type_name_begin identifier @type_name_end type_pointer_suffix? (")" | separators_till_nl)
     {
         if (parser.current_meta_storage.has_dynamic_array_type)
         {
             fprintf (stderr, "[%s:%lu:%lu] Encountered duplicate dynamic array type meta.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line - 1u, (unsigned long) parser.cursor_symbol - 1u);
             return PARSE_STATUS_FAILED;
         }

         parser.current_meta_storage.has_dynamic_array_type = KAN_TRUE;
         ASSEMBLE_TYPE_INFO (parser.current_meta_storage.dynamic_array_type);
         return PARSE_STATUS_IN_PROGRESS;
     }

     ("pragma" separator_no_nl+)? "kan_reflection_size_field" separator_no_nl+
     @name_begin identifier @name_end (")" | separators_till_nl)
     {
         if (parser.current_meta_storage.size_field)
         {
             fprintf (stderr, "[%s:%lu:%lu] Encountered duplicate size field meta.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line - 1u, (unsigned long) parser.cursor_symbol - 1u);
             return PARSE_STATUS_FAILED;
         }

         parser.current_meta_storage.size_field = kan_char_sequence_intern (name_begin, name_end);
         return PARSE_STATUS_IN_PROGRESS;
     }

     ("pragma" separator_no_nl+)? "kan_reflection_visibility_condition_field" separator_no_nl+
     @name_begin identifier @name_end (")" | separators_till_nl)
     {
         if (parser.current_meta_storage.visibility_condition_field)
         {
             fprintf (stderr, "[%s:%lu:%lu] Encountered duplicate visibility condition field meta.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line - 1u, (unsigned long) parser.cursor_symbol - 1u);
             return PARSE_STATUS_FAILED;
         }

         parser.current_meta_storage.visibility_condition_field = kan_char_sequence_intern (name_begin, name_end);
         return PARSE_STATUS_IN_PROGRESS;
     }

     ("pragma" separator_no_nl+)? "kan_reflection_visibility_condition_value" separator_no_nl+
     @value_begin (. \ [\(\)])+ @value_end (")" | separators_till_nl)
     {
         struct visibility_condition_value_node_t *node =
             kan_allocate_batched (global.meta_allocation_group, sizeof (struct visibility_condition_value_node_t));

         node->next = parser.current_meta_storage.first_visibility_condition_value;
         node->value = kan_char_sequence_intern (value_begin, value_end);
         parser.current_meta_storage.first_visibility_condition_value = node;
         return PARSE_STATUS_IN_PROGRESS;
     }

     ("pragma" separator_no_nl+)? "kan_reflection_enum_meta" separator_no_nl+
     @name_begin identifier @name_end (")" | separators_till_nl)
     {
         struct top_level_meta_t *node =
             kan_allocate_batched (global.meta_allocation_group, sizeof (struct top_level_meta_t));

         node->next = parser.current_meta_storage.first_enum_meta;
         node->top_level_name = kan_char_sequence_intern (name_begin, name_end);
         parser.current_meta_storage.first_enum_meta = node;
         return PARSE_STATUS_IN_PROGRESS;
     }

     ("pragma" separator_no_nl+)? "kan_reflection_enum_value_meta" separator_no_nl+
     @name_begin identifier @name_end separator_no_nl+ @value_begin identifier @value_end (")" | separators_till_nl)
     {
         struct secondary_level_meta_t *node =
             kan_allocate_batched (global.meta_allocation_group, sizeof (struct secondary_level_meta_t));

         node->next = parser.current_meta_storage.first_enum_value_meta;
         node->top_level_name = kan_char_sequence_intern (name_begin, name_end);
         node->secondary_level_name = kan_char_sequence_intern (value_begin, value_end);
         parser.current_meta_storage.first_enum_value_meta = node;
         return PARSE_STATUS_IN_PROGRESS;
     }

     ("pragma" separator_no_nl+)? "kan_reflection_struct_meta" separator_no_nl+
     @name_begin identifier @name_end (")" | separators_till_nl)
     {
         struct top_level_meta_t *node =
             kan_allocate_batched (global.meta_allocation_group, sizeof (struct top_level_meta_t));

         node->next = parser.current_meta_storage.first_struct_meta;
         node->top_level_name = kan_char_sequence_intern (name_begin, name_end);
         parser.current_meta_storage.first_struct_meta = node;
         return PARSE_STATUS_IN_PROGRESS;
     }

     ("pragma" separator_no_nl+)? "kan_reflection_struct_field_meta" separator_no_nl+
     @name_begin identifier @name_end separator_no_nl+ @value_begin identifier @value_end (")" | separators_till_nl)
     {
         struct secondary_level_meta_t *node =
             kan_allocate_batched (global.meta_allocation_group, sizeof (struct secondary_level_meta_t));

         node->next = parser.current_meta_storage.first_struct_field_meta;
         node->top_level_name = kan_char_sequence_intern (name_begin, name_end);
         node->secondary_level_name = kan_char_sequence_intern (value_begin, value_end);
         parser.current_meta_storage.first_struct_field_meta = node;
         return PARSE_STATUS_IN_PROGRESS;
     }

     ("pragma" separator_no_nl+)? "kan_reflection_function_meta" separator_no_nl+
     @name_begin identifier @name_end (")" | separators_till_nl)
     {
         struct top_level_meta_t *node =
             kan_allocate_batched (global.meta_allocation_group, sizeof (struct top_level_meta_t));

         node->next = parser.current_meta_storage.first_function_meta;
         node->top_level_name = kan_char_sequence_intern (name_begin, name_end);
         parser.current_meta_storage.first_function_meta = node;
         return PARSE_STATUS_IN_PROGRESS;
     }

     ("pragma" separator_no_nl+)? "kan_reflection_function_argument_meta" separator_no_nl+
     @name_begin identifier @name_end separator_no_nl+ @value_begin identifier @value_end (")" | separators_till_nl)
     {
         struct secondary_level_meta_t *node =
             kan_allocate_batched (global.meta_allocation_group, sizeof (struct secondary_level_meta_t));

         node->next = parser.current_meta_storage.first_function_argument_meta;
         node->top_level_name = kan_char_sequence_intern (name_begin, name_end);
         node->secondary_level_name = kan_char_sequence_intern (value_begin, value_end);
         parser.current_meta_storage.first_function_argument_meta = node;
         return PARSE_STATUS_IN_PROGRESS;
     }

     "warning (push, " [0-9]+ "))" { return PARSE_STATUS_IN_PROGRESS; }

     "warning (pop))" { return PARSE_STATUS_IN_PROGRESS; }

     (. \ [\)])+ (")" | separators_till_nl) { return PARSE_STATUS_IN_PROGRESS; }

     *
     {
         fprintf (stderr, "[%s:%lu:%lu] Unknown expression. Expected pragma.\n",
             parser.current_target_node ? parser.current_target_node->path : "<unknown>",
             (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
         return PARSE_STATUS_FAILED;
     }

     $
     {
         fprintf (stderr, "[%s:%lu:%lu] Reached end of the input while expecting pragma.\n",
             parser.current_target_node ? parser.current_target_node->path : "<unknown>",
             (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
         return PARSE_STATUS_FAILED;
     }
     */

    return PARSE_STATUS_FAILED;
}

static enum parse_status_t parse_enum_declaration (const char *declaration_name_begin,
                                                   const char *declaration_name_end);

static enum parse_status_t parse_struct_declaration (const char *declaration_name_begin,
                                                     const char *declaration_name_end);

static enum parse_status_t parse_function_declaration (struct type_info_t *return_type_info,
                                                       const char *declaration_name_begin,
                                                       const char *declaration_name_end,
                                                       const char *whole_return_type_begin,
                                                       const char *whole_return_type_end,
                                                       const char *full_declaration_begin,
                                                       const char *full_declaration_end);

static enum parse_status_t process_parsed_symbol (const char *declaration_name_begin,
                                                  const char *declaration_name_end,
                                                  struct type_info_t *type,
                                                  const char *symbol_info_begin,
                                                  const char *symbol_info_end);

static enum parse_status_t parse_skip_until_curly_braces_close (void);
static enum parse_status_t parse_skip_until_round_braces_close (void);

static enum parse_status_t parse_main (void)
{
    TYPE_MARKERS;
    const char *name_begin;
    const char *name_end;
    KAN_MUTE_UNUSED_WARNINGS_BEGIN
    const char *array_size_begin;
    const char *array_size_end;
    KAN_MUTE_UNUSED_WARNINGS_END
    const char *symbol_info_begin;
    const char *symbol_info_end;
    const char *whole_type_begin;
    const char *whole_type_end;
    const char *function_full_declaration_begin;
    const char *function_full_declaration_end;

    if (!parser.current_target_node ||
        (parser.current_target_node->found_in_input_index != KAN_INT_MAX (kan_instance_size_t) &&
         parser.current_target_node->found_in_input_index != global.current_input_index))
    {
        const char *line_begin;
        const char *line_end;
        const char *file_begin;
        const char *file_end;

        while (KAN_TRUE)
        {
            parser.token = parser.cursor;
            /*!re2c
             line_pragma
             {
                 parser_enter_file (file_begin, file_end, line_begin, line_end);
                 return PARSE_STATUS_IN_PROGRESS;
             }

             * { continue; }

             $ { return PARSE_STATUS_FINISHED; }
             */
        }
    }

    while (KAN_TRUE)
    {
        parser.token = parser.cursor;
        /*!re2c
         "#" | "__pragma("
         {
             return continue_into_potential_pragma (KAN_TRUE);
         }

         "typedef" ((. \ [;\{\}]) | separator)+ ";"
         {
             // Simple typedef without braces. Add it to declarations if we're parsing target object file.
             if (!meta_storage_is_empty (&parser.current_meta_storage))
             {
                 fprintf (stderr, "[%s:%lu:%lu] Encountered typedef after meta, typedefs do not support any meta.\n",
                     parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                     (unsigned long) parser.current_target_line - 1u, (unsigned long) parser.cursor_symbol - 1u);
                 return PARSE_STATUS_FAILED;
             }

             if (parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT)
             {
                 kan_trivial_string_buffer_append_char_sequence (&global.declaration_section, parser.token,
                     (kan_instance_size_t) (parser.cursor - parser.token));
                 kan_trivial_string_buffer_append_string (&global.declaration_section, "\n");
             }

             continue;
         }

         "enum" separator+ @name_begin identifier @name_end separator* "{"
         {
             return parse_enum_declaration (name_begin, name_end);
         }

         "struct" separator+ @name_begin identifier @name_end separator* "{"
         {
             return parse_struct_declaration (name_begin, name_end);
         }

         ("__declspec(" [a-zA-Z0-9]+ ")" separator+)?
         @function_full_declaration_begin ("extern" separator+)? ("static" separator+)? ("inline" separator+)?
         @whole_type_begin type @whole_type_end
         @name_begin identifier @name_end separator* "(" @function_full_declaration_end
         {
             struct type_info_t return_type;
             ASSEMBLE_TYPE_INFO (return_type)
             return parse_function_declaration (&return_type, name_begin, name_end, whole_type_begin, whole_type_end,
                 function_full_declaration_begin, function_full_declaration_end);
         }

         ("__declspec(" [a-zA-Z0-9]+ ")" separator+)? ("extern" separator+)? ("static" separator+)?
         @symbol_info_begin type @name_begin identifier @name_end separator* array_suffix? @symbol_info_end
         (separator* "=" separator* ((. \ [;]) | separator)+)? separator* ";"
         {
             struct type_info_t type;
             ASSEMBLE_TYPE_INFO (type)
             return process_parsed_symbol (name_begin, name_end, &type, symbol_info_begin, symbol_info_end);
         }

         "__declspec(align(" [0-9]+ "))"
         {
             // Due to how lines work when using MSVC preprocessor, alignment declspec might be dangling.
             // But it is okay, we do not need it and can skip it.
             continue;
         }

         "{"
         {
             // Due to how we parse, there is a dangling block. We're not interested in it.
             return parse_skip_until_curly_braces_close ();
         }

         "_Static_assert" separator* "("
         {
             // Some static assert, reflection is not interested in it.
             return parse_skip_until_round_braces_close ();
         }

         ";"
         {
             // Due to how we parse, there is a dangling semicolon. We're not interested in it.
             continue;
         }

         separator { continue; }

         *
         {
             fprintf (stderr, "[%s:%lu:%lu] Unknown expression. Expected pragma/typedef/enum/struct/function/symbol.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
             return PARSE_STATUS_FAILED;
         }

         $ { return PARSE_STATUS_FINISHED; }
         */
    }
}

#define INCOMPATIBLE_WITH_META(CHECK, WHO, WHICH_META)                                                                 \
    if (CHECK)                                                                                                         \
    {                                                                                                                  \
        fprintf (stderr, "[%s:%lu:%lu] %s are not compatible with %s meta.\n",                                         \
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",                          \
                 (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u, WHO,           \
                 WHICH_META);                                                                                          \
        return PARSE_STATUS_FAILED;                                                                                    \
    }

struct enum_reflection_context_t
{
    kan_bool_t reflected;
    kan_bool_t flags;
    kan_instance_size_t reflected_values_count;
    char *name;
};

static inline void finish_enum_generation (struct enum_reflection_context_t *context)
{
    if (!context->reflected || context->reflected_values_count == 0u)
    {
        return;
    }

    kan_trivial_string_buffer_append_string (&global.generated_symbols_section,
                                             "static struct kan_reflection_enum_t reflection_");
    kan_trivial_string_buffer_append_string (&global.generated_symbols_section, context->name);
    kan_trivial_string_buffer_append_string (&global.generated_symbols_section, "_data;\n");

    kan_trivial_string_buffer_append_string (&global.generated_symbols_section,
                                             "static struct kan_reflection_enum_value_t reflection_");
    kan_trivial_string_buffer_append_string (&global.generated_symbols_section, context->name);
    kan_trivial_string_buffer_append_string (&global.generated_symbols_section, "_values[");
    kan_trivial_string_buffer_append_unsigned_long (&global.generated_symbols_section,
                                                    (unsigned long) context->reflected_values_count);
    kan_trivial_string_buffer_append_string (&global.generated_symbols_section, "];\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "    reflection_");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_data = (struct kan_reflection_enum_t) {\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .name = kan_string_intern (\"");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "\"),\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .flags = ");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->flags ? "KAN_TRUE" : "KAN_FALSE");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, ",\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .values_count = ");
    kan_trivial_string_buffer_append_unsigned_long (&global.bootstrap_section,
                                                    (unsigned long) context->reflected_values_count);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "u,\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .values = reflection_");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_values,\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "    };\n\n");

    kan_trivial_string_buffer_append_string (&global.registrar_section,
                                             "    success = kan_reflection_registry_add_enum (registry, &reflection_");
    kan_trivial_string_buffer_append_string (&global.registrar_section, context->name);
    kan_trivial_string_buffer_append_string (&global.registrar_section, "_data);\n");
    kan_trivial_string_buffer_append_string (&global.registrar_section, "    KAN_ASSERT (success)\n");
}

static inline enum parse_status_t process_enum_value (struct enum_reflection_context_t *context,
                                                      const char *name_begin,
                                                      const char *name_end)
{
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.export, "Enum values", "export")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.flags, "Enum values", "flags")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.external_pointer, "Enum values", "external pointer")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.has_dynamic_array_type, "Enum values", "dynamic array type")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.explicit_init_functor, "Enum values", "explicit init functor")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.explicit_shutdown_functor, "Enum values",
                            "explicit shutdown functor")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.size_field, "Enum values", "size field")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.visibility_condition_field, "Enum values",
                            "visibility condition field")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_visibility_condition_value, "Enum values",
                            "visibility condition value")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_enum_meta, "Enum values", "enum meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_enum_value_meta, "Enum values",
                            "enum value meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_struct_meta, "Enum values", "struct meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_struct_field_meta, "Enum values",
                            "struct field meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_function_meta, "Enum values", "function meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_function_argument_meta, "Enum values",
                            "function argument meta attachment")

    if (!context->reflected || parser.current_meta_storage.ignore)
    {
        meta_storage_shutdown (&parser.current_meta_storage);
        meta_storage_init (&parser.current_meta_storage);
        return PARSE_STATUS_IN_PROGRESS;
    }

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "    reflection_");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_values[");
    kan_trivial_string_buffer_append_unsigned_long (&global.bootstrap_section,
                                                    (unsigned long) context->reflected_values_count);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                             "u] = (struct kan_reflection_enum_value_t) {\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .name = kan_string_intern (\"");
    kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, name_begin,
                                                    (kan_instance_size_t) (name_end - name_begin));
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "\"),\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                             "        .value = (kan_reflection_enum_size_t) ");
    kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, name_begin,
                                                    (kan_instance_size_t) (name_end - name_begin));
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, ",\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "    };\n\n");

    ++context->reflected_values_count;
    meta_storage_shutdown (&parser.current_meta_storage);
    meta_storage_init (&parser.current_meta_storage);
    return PARSE_STATUS_IN_PROGRESS;
}

static enum parse_status_t parse_enum_declaration (const char *declaration_name_begin, const char *declaration_name_end)
{
    if (parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT)
    {
        kan_trivial_string_buffer_append_char_sequence (&global.declaration_section, parser.token,
                                                        (kan_instance_size_t) (parser.cursor - parser.token));
    }

    struct enum_reflection_context_t context = {
        .reflected = !parser.current_meta_storage.ignore,
        .flags = parser.current_meta_storage.flags,
        .reflected_values_count = 0u,
        .name = NULL,
    };

    if (context.reflected)
    {
        context.name = kan_stack_group_allocator_allocate (
            &global.persistent_allocator, 1u + (declaration_name_end - declaration_name_begin), _Alignof (char));
        memcpy (context.name, declaration_name_begin, declaration_name_end - declaration_name_begin);
        context.name[declaration_name_end - declaration_name_begin] = '\0';
    }

    INCOMPATIBLE_WITH_META (parser.current_meta_storage.export, "Enums", "export")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.external_pointer, "Enums", "external pointer")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.has_dynamic_array_type, "Enums", "dynamic array type")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.explicit_init_functor, "Enums", "explicit init functor")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.explicit_shutdown_functor, "Enums", "explicit shutdown functor")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.size_field, "Enums", "size field")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.visibility_condition_field, "Enums",
                            "visibility condition field")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_visibility_condition_value, "Enums",
                            "visibility condition value")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_enum_meta, "Enums", "enum meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_enum_value_meta, "Enums", "enum value meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_struct_meta, "Enums", "struct meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_struct_field_meta, "Enums",
                            "struct field meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_function_meta, "Enums", "function meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_function_argument_meta, "Enums",
                            "function argument meta attachment")

    meta_storage_shutdown (&parser.current_meta_storage);
    meta_storage_init (&parser.current_meta_storage);

    const char *name_begin;
    const char *name_end;

    while (KAN_TRUE)
    {
        parser.token = parser.cursor;
        /*!re2c
         "#" | "__pragma("
         {
             if (continue_into_potential_pragma (KAN_FALSE) == PARSE_STATUS_FAILED)
             {
                 return PARSE_STATUS_FAILED;
             }

             continue;
         }

         @name_begin identifier @name_end
         (separator* "=" separator* ([0-9a-zA-Z_+<>] | separator | "-")+)? separator* ","?
         {
             if (parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT)
             {
                 kan_trivial_string_buffer_append_char_sequence (&global.declaration_section, parser.token,
                     (kan_instance_size_t) (parser.cursor - parser.token));
             }

             if (process_enum_value (&context, name_begin, name_end) == PARSE_STATUS_FAILED)
             {
                 return PARSE_STATUS_FAILED;
             }

             continue;
         }

         separator+
         {
             if (parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT)
             {
                 kan_trivial_string_buffer_append_char_sequence (&global.declaration_section, parser.token,
                     (kan_instance_size_t) (parser.cursor - parser.token));
             }

             continue;
         }

         "}" separator*
         {
             // We do not require semicolon as, due to how MSVC works, it might be after line directive.
             if (parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT)
             {
                 kan_trivial_string_buffer_append_char_sequence (&global.declaration_section, parser.token,
                     (kan_instance_size_t) (parser.cursor - parser.token));
                 kan_trivial_string_buffer_append_string (&global.declaration_section, ";\n");
             }

             finish_enum_generation (&context);
             return PARSE_STATUS_IN_PROGRESS;
         }

         *
         {
             fprintf (stderr, "[%s:%lu:%lu] Unknown expression. Expected enum value.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
             return PARSE_STATUS_FAILED;
         }

         $
         {
             fprintf (stderr, "[%s:%lu:%lu] Reached end while reading enum values.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
             return PARSE_STATUS_FAILED;
         }
         */
    }
}

struct struct_reflection_context_t
{
    kan_bool_t reflected;
    kan_instance_size_t reflected_fields_count;
    char *name;
};

static inline void finish_struct_generation (struct struct_reflection_context_t *context)
{
    if (!context->reflected || context->reflected_fields_count == 0u)
    {
        return;
    }

    kan_trivial_string_buffer_append_string (&global.generated_symbols_section,
                                             "static struct kan_reflection_struct_t reflection_");
    kan_trivial_string_buffer_append_string (&global.generated_symbols_section, context->name);
    kan_trivial_string_buffer_append_string (&global.generated_symbols_section, "_data;\n");

    kan_trivial_string_buffer_append_string (&global.generated_symbols_section,
                                             "static struct kan_reflection_field_t reflection_");
    kan_trivial_string_buffer_append_string (&global.generated_symbols_section, context->name);
    kan_trivial_string_buffer_append_string (&global.generated_symbols_section, "_fields[");
    kan_trivial_string_buffer_append_unsigned_long (&global.generated_symbols_section,
                                                    (unsigned long) context->reflected_fields_count);
    kan_trivial_string_buffer_append_string (&global.generated_symbols_section, "u];\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "    reflection_");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_data = (struct kan_reflection_struct_t) {\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .name = kan_string_intern (\"");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "\"),\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .size = sizeof (struct ");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "),\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .alignment = _Alignof (struct ");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "),\n");

    kan_instance_size_t struct_data_name_length = (kan_instance_size_t) strlen (context->name);
    if (struct_data_name_length > 2u && context->name[struct_data_name_length - 2u] == '_' &&
        context->name[struct_data_name_length - 1u] == 't')
    {
        struct_data_name_length -= 2u;
    }

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "#if !defined(");
    kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, context->name, struct_data_name_length);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_init_lifetime_functor)\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "#    define ");
    kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, context->name, struct_data_name_length);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_init_lifetime_functor NULL\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "#endif\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .init = ");
    kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, context->name, struct_data_name_length);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_init_lifetime_functor,\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "#if !defined(");
    kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, context->name, struct_data_name_length);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_shutdown_lifetime_functor)\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "#    define ");
    kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, context->name, struct_data_name_length);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_shutdown_lifetime_functor NULL\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "#endif\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .shutdown = ");
    kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, context->name, struct_data_name_length);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_shutdown_lifetime_functor,\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .functor_user_data = 0u,\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .fields_count = ");
    kan_trivial_string_buffer_append_unsigned_long (&global.bootstrap_section,
                                                    (unsigned long) context->reflected_fields_count);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "u,\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .fields = reflection_");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_fields,\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "    };\n\n");

    kan_trivial_string_buffer_append_string (
        &global.registrar_section, "    success = kan_reflection_registry_add_struct (registry, &reflection_");
    kan_trivial_string_buffer_append_string (&global.registrar_section, context->name);
    kan_trivial_string_buffer_append_string (&global.registrar_section, "_data);\n");
    kan_trivial_string_buffer_append_string (&global.registrar_section, "    KAN_ASSERT (success)\n");
}

static inline enum parse_status_t struct_field_bootstrap_array_common_internals (struct type_info_t *type)
{
    if (type->pointer_level > 0u)
    {
        if (parser.current_meta_storage.external_pointer || type->group == TYPE_INFO_GROUP_ENUM ||
            (type->group == TYPE_INFO_GROUP_VALUE && type->name != interned.type_char))
        {
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     "            .item_archetype = "
                                                     "KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER,\n");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     "            .item_size = sizeof (void *),\n");
        }
        else if (type->group == TYPE_INFO_GROUP_VALUE && type->name == interned.type_char && type->pointer_level == 1u)
        {
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     "            .item_archetype = "
                                                     "KAN_REFLECTION_ARCHETYPE_STRING_POINTER,\n");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     "            .item_size = sizeof (void *),\n");
        }
        else if (type->group == TYPE_INFO_GROUP_STRUCT && type->pointer_level == 1u)
        {
            kan_trivial_string_buffer_append_string (
                &global.bootstrap_section, "            .item_archetype = KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER,\n");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     "            .item_size = sizeof (void *),\n");

            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     "            .item_archetype_struct_pointer = {.type_name = "
                                                     "kan_string_intern "
                                                     "(\"");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, type->name);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, "\")},\n");
        }
        else
        {
            fprintf (stderr, "[%s:%lu:%lu] Unable to properly convert array item type to reflection data.\n",
                     parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                     (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
            return PARSE_STATUS_FAILED;
        }
    }
    else
    {
        switch (type->group)
        {
        case TYPE_INFO_GROUP_VALUE:
            if (type->name == interned.type_interned_string)
            {
                kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                         "            .item_archetype = "
                                                         "KAN_REFLECTION_ARCHETYPE_INTERNED_STRING,\n");
            }
            else if (type->name == interned.type_patch)
            {
                kan_trivial_string_buffer_append_string (
                    &global.bootstrap_section, "            .item_archetype = KAN_REFLECTION_ARCHETYPE_PATCH,\n");
            }
            else
            {
                kan_trivial_string_buffer_append_string (
                    &global.bootstrap_section, "            .item_archetype = ARCHETYPE_SELECTION_HELPER_GENERIC (*(");
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, type->name);
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, " *) NULL),\n");
            }

            kan_trivial_string_buffer_append_string (&global.bootstrap_section, "            .item_size = sizeof (");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, type->name);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, "),\n");
            break;

        case TYPE_INFO_GROUP_ENUM:
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     "            .item_archetype = "
                                                     "KAN_REFLECTION_ARCHETYPE_ENUM,\n");
            kan_trivial_string_buffer_append_string (
                &global.bootstrap_section, "            .item_archetype_enum = {.type_name = kan_string_intern (\"");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, type->name);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, "\")},\n");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     "            .item_size = sizeof (enum \n");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, type->name);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, "),\n");
            break;

        case TYPE_INFO_GROUP_STRUCT:
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     "            .item_archetype = "
                                                     "KAN_REFLECTION_ARCHETYPE_STRUCT,\n");
            kan_trivial_string_buffer_append_string (
                &global.bootstrap_section, "            .item_archetype_struct = {.type_name = kan_string_intern (\"");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, type->name);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, "\")},\n");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     "            .item_size = sizeof (struct ");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, type->name);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, "),\n");
            break;
        }
    }

    return PARSE_STATUS_IN_PROGRESS;
}

static inline enum parse_status_t process_struct_field (struct struct_reflection_context_t *context,
                                                        const char *name_begin,
                                                        const char *name_end,
                                                        struct type_info_t *type,
                                                        const char *array_size_begin,
                                                        const char *array_size_end)
{
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.export, "Struct fields", "export")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.flags, "Struct fields", "flags")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.explicit_init_functor, "Struct fields", "explicit init functor")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.explicit_shutdown_functor, "Struct fields",
                            "explicit shutdown functor")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_enum_meta, "Struct fields", "enum meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_enum_value_meta, "Struct fields",
                            "enum value meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_struct_meta, "Struct fields", "struct meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_struct_field_meta, "Struct fields",
                            "struct field meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_function_meta, "Struct fields",
                            "function meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_function_argument_meta, "Struct fields",
                            "function argument meta attachment")

    if (!array_size_begin)
    {
        INCOMPATIBLE_WITH_META (parser.current_meta_storage.export, "Non-array struct fields", "size field")
    }

    if (!context->reflected || parser.current_meta_storage.ignore)
    {
        meta_storage_shutdown (&parser.current_meta_storage);
        meta_storage_init (&parser.current_meta_storage);
        return PARSE_STATUS_IN_PROGRESS;
    }

    struct visibility_condition_value_node_t *condition_value =
        parser.current_meta_storage.first_visibility_condition_value;

    if (condition_value)
    {
        if (!parser.current_meta_storage.visibility_condition_field)
        {
            fprintf (
                stderr,
                "[%s:%lu:%lu] Field has visibility condition value meta, but not visibility condition field meta.\n",
                parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
            return PARSE_STATUS_FAILED;
        }

        kan_trivial_string_buffer_append_string (&global.generated_symbols_section,
                                                 "static kan_reflection_visibility_size_t reflection_");
        kan_trivial_string_buffer_append_string (&global.generated_symbols_section, context->name);
        kan_trivial_string_buffer_append_string (&global.generated_symbols_section, "_field_");
        kan_trivial_string_buffer_append_char_sequence (&global.generated_symbols_section, name_begin,
                                                        (kan_instance_size_t) (name_end - name_begin));
        kan_trivial_string_buffer_append_string (&global.generated_symbols_section, "_visibility_values[] = {");

        while (condition_value)
        {
            kan_trivial_string_buffer_append_string (&global.generated_symbols_section,
                                                     "(kan_reflection_visibility_size_t) ");
            kan_trivial_string_buffer_append_string (&global.generated_symbols_section, condition_value->value);

            if (condition_value->next)
            {
                kan_trivial_string_buffer_append_string (&global.generated_symbols_section, ", ");
            }

            condition_value = condition_value->next;
        }

        kan_trivial_string_buffer_append_string (&global.generated_symbols_section, "};\n");
    }

    kan_trivial_string_buffer_append_string (&global.generation_control_section, "#define ");
    kan_trivial_string_buffer_append_string (&global.generation_control_section, context->name);
    kan_trivial_string_buffer_append_string (&global.generation_control_section, "_");
    kan_trivial_string_buffer_append_char_sequence (&global.generation_control_section, name_begin,
                                                    (kan_instance_size_t) (name_end - name_begin));
    kan_trivial_string_buffer_append_string (&global.generation_control_section, "_field_index ");
    kan_trivial_string_buffer_append_unsigned_long (&global.generation_control_section,
                                                    (unsigned long) context->reflected_fields_count);
    kan_trivial_string_buffer_append_string (&global.generation_control_section, "\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "    reflection_");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_fields[");
    kan_trivial_string_buffer_append_unsigned_long (&global.bootstrap_section,
                                                    (unsigned long) context->reflected_fields_count);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "u] = (struct kan_reflection_field_t) {\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .name = kan_string_intern (\"");
    kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, name_begin,
                                                    (kan_instance_size_t) (name_end - name_begin));
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "\"),\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .offset = offsetof (struct ");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, ", ");
    kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, name_begin,
                                                    (kan_instance_size_t) (name_end - name_begin));
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "),\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .size = SIZE_OF_FIELD (");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, ", ");
    kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, name_begin,
                                                    (kan_instance_size_t) (name_end - name_begin));
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "),\n");

    if (array_size_begin)
    {
        if (array_size_begin == array_size_end)
        {
            fprintf (stderr, "[%s:%lu:%lu] Field is an runtime sized array which is not supported for reflection.\n",
                     parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                     (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
            return PARSE_STATUS_FAILED;
        }

        kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                 "        .archetype = KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY,\n");
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .archetype_inline_array = {\n");

        if (struct_field_bootstrap_array_common_internals (type) == PARSE_STATUS_FAILED)
        {
            return PARSE_STATUS_FAILED;
        }

        kan_trivial_string_buffer_append_string (&global.bootstrap_section, "            .item_count = ");
        kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, array_size_begin,
                                                        (kan_instance_size_t) (array_size_end - array_size_begin));
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, ",\n");

        if (parser.current_meta_storage.size_field)
        {
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     "            .size_field = reflection_");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_fields + ");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, parser.current_meta_storage.size_field);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_field_index,\n");
        }
        else
        {
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, "            .size_field = NULL,\n");
        }

        kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        },\n");
    }
    else if (type->pointer_level > 0u)
    {
        switch (type->group)
        {
        case TYPE_INFO_GROUP_VALUE:
            if (!parser.current_meta_storage.external_pointer && type->name == interned.type_char &&
                type->pointer_level == 1u)
            {
                kan_trivial_string_buffer_append_string (
                    &global.bootstrap_section, "        .archetype = KAN_REFLECTION_ARCHETYPE_STRING_POINTER,\n");
            }
            else
            {
                kan_trivial_string_buffer_append_string (
                    &global.bootstrap_section, "        .archetype = KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER,\n");
            }

            break;

        case TYPE_INFO_GROUP_ENUM:
            // Currently pointers to enums are processed as externals.
            kan_trivial_string_buffer_append_string (
                &global.bootstrap_section, "        .archetype = KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER,\n");
            break;

        case TYPE_INFO_GROUP_STRUCT:
            if (parser.current_meta_storage.external_pointer || type->pointer_level > 1u)
            {
                kan_trivial_string_buffer_append_string (
                    &global.bootstrap_section, "        .archetype = KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER,\n");
            }
            else
            {
                kan_trivial_string_buffer_append_string (
                    &global.bootstrap_section, "        .archetype = KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER,\n");
                kan_trivial_string_buffer_append_string (
                    &global.bootstrap_section,
                    "        .archetype_struct_pointer = {.type_name = kan_string_intern (\"");
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, type->name);
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, "\")},\n");
            }

            break;
        }
    }
    else
    {
        switch (type->group)
        {
        case TYPE_INFO_GROUP_VALUE:
            if (type->name == interned.type_interned_string)
            {
                kan_trivial_string_buffer_append_string (
                    &global.bootstrap_section, "        .archetype = KAN_REFLECTION_ARCHETYPE_INTERNED_STRING,\n");
            }
            else if (type->name == interned.type_patch)
            {
                kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                         "        .archetype = KAN_REFLECTION_ARCHETYPE_PATCH,\n");
            }
            else
            {
                kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                         "        .archetype = ARCHETYPE_SELECTION_HELPER (");
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, ", ");
                kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, name_begin,
                                                                (kan_instance_size_t) (name_end - name_begin));
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, "),\n");
            }

            break;

        case TYPE_INFO_GROUP_ENUM:
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     "        .archetype = KAN_REFLECTION_ARCHETYPE_ENUM,\n");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     "        .archetype_enum = {.type_name = kan_string_intern (\"");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, type->name);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, "\")},\n");
            break;

        case TYPE_INFO_GROUP_STRUCT:
            if (type->name == interned.type_dynamic_array)
            {
                kan_trivial_string_buffer_append_string (
                    &global.bootstrap_section, "        .archetype = KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY,\n");
                kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                         "        .archetype_dynamic_array = {\n");

                if (!parser.current_meta_storage.has_dynamic_array_type)
                {
                    fprintf (stderr, "[%s:%lu:%lu] Dynamic array field has no dynamic array type meta.\n",
                             parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                             (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
                    return PARSE_STATUS_FAILED;
                }

                if (struct_field_bootstrap_array_common_internals (&parser.current_meta_storage.dynamic_array_type) ==
                    PARSE_STATUS_FAILED)
                {
                    return PARSE_STATUS_FAILED;
                }

                kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        },\n");
            }
            else
            {
                kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                         "        .archetype = KAN_REFLECTION_ARCHETYPE_STRUCT,\n");
                kan_trivial_string_buffer_append_string (
                    &global.bootstrap_section, "        .archetype_struct = {.type_name = kan_string_intern (\"");
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, type->name);
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, "\")},\n");
            }

            break;
        }
    }

    if (parser.current_meta_storage.visibility_condition_field)
    {
        kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                 "        .visibility_condition_field = reflection_");
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_fields + ");
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_");
        kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                 parser.current_meta_storage.visibility_condition_field);
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_field_index,\n");

        kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                 "        .visibility_condition_values_count = sizeof (reflection_");
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_field_");
        kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, name_begin,
                                                        (kan_instance_size_t) (name_end - name_begin));
        kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                 "_visibility_values) / sizeof (kan_reflection_visibility_size_t),\n");

        kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                 "        .visibility_condition_values = reflection_");
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_field_");
        kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, name_begin,
                                                        (kan_instance_size_t) (name_end - name_begin));
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_visibility_values,\n");
    }
    else
    {
        kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                 "        .visibility_condition_field = NULL,\n");
        kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                 "        .visibility_condition_values_count = 0u,\n");
        kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                 "        .visibility_condition_values = NULL,\n");
    }

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "    };\n\n");
    ++context->reflected_fields_count;
    meta_storage_shutdown (&parser.current_meta_storage);
    meta_storage_init (&parser.current_meta_storage);
    return PARSE_STATUS_IN_PROGRESS;
}

static enum parse_status_t parse_struct_declaration (const char *declaration_name_begin,
                                                     const char *declaration_name_end)
{
    if (parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT)
    {
        kan_trivial_string_buffer_append_char_sequence (&global.declaration_section, parser.token,
                                                        (kan_instance_size_t) (parser.cursor - parser.token));
    }

    struct struct_reflection_context_t context = {
        .reflected = !parser.current_meta_storage.ignore,
        .reflected_fields_count = 0u,
        .name = NULL,
    };

    if (context.reflected)
    {
        context.name = kan_stack_group_allocator_allocate (
            &global.persistent_allocator, 1u + (declaration_name_end - declaration_name_begin), _Alignof (char));
        memcpy (context.name, declaration_name_begin, declaration_name_end - declaration_name_begin);
        context.name[declaration_name_end - declaration_name_begin] = '\0';

        kan_instance_size_t name_length_without_suffix =
            (kan_instance_size_t) (declaration_name_end - declaration_name_begin);
        if (name_length_without_suffix > 2u && declaration_name_begin[name_length_without_suffix - 2u] == '_' &&
            declaration_name_begin[name_length_without_suffix - 1u] == 't')
        {
            name_length_without_suffix -= 2u;
        }

        if (parser.current_meta_storage.explicit_init_functor)
        {
            kan_trivial_string_buffer_append_string (&global.generation_control_section, "#define ");
            kan_trivial_string_buffer_append_char_sequence (&global.generation_control_section, context.name,
                                                            name_length_without_suffix);
            kan_trivial_string_buffer_append_string (&global.generation_control_section,
                                                     "_init_lifetime_functor lifetime_functor_");
            kan_trivial_string_buffer_append_string (&global.generation_control_section,
                                                     parser.current_meta_storage.explicit_init_functor);
            kan_trivial_string_buffer_append_string (&global.generation_control_section, "\n");

            kan_trivial_string_buffer_append_string (&global.generated_functions_section,
                                                     "static void lifetime_functor_");
            kan_trivial_string_buffer_append_string (&global.generated_functions_section,
                                                     parser.current_meta_storage.explicit_init_functor);
            kan_trivial_string_buffer_append_string (
                &global.generated_functions_section,
                " (kan_functor_user_data_t user_data, void *generic_instance)\n{\n    ");

            kan_trivial_string_buffer_append_string (&global.generated_functions_section,
                                                     parser.current_meta_storage.explicit_init_functor);
            kan_trivial_string_buffer_append_string (&global.generated_functions_section,
                                                     " (generic_instance);\n}\n\n");
        }

        if (parser.current_meta_storage.explicit_shutdown_functor)
        {
            kan_trivial_string_buffer_append_string (&global.generation_control_section, "#define ");
            kan_trivial_string_buffer_append_char_sequence (&global.generation_control_section, context.name,
                                                            name_length_without_suffix);
            kan_trivial_string_buffer_append_string (&global.generation_control_section,
                                                     "_shutdown_lifetime_functor lifetime_functor_");
            kan_trivial_string_buffer_append_string (&global.generation_control_section,
                                                     parser.current_meta_storage.explicit_shutdown_functor);
            kan_trivial_string_buffer_append_string (&global.generation_control_section, "\n");

            kan_trivial_string_buffer_append_string (&global.generated_functions_section,
                                                     "static void lifetime_functor_");
            kan_trivial_string_buffer_append_string (&global.generated_functions_section,
                                                     parser.current_meta_storage.explicit_shutdown_functor);
            kan_trivial_string_buffer_append_string (
                &global.generated_functions_section,
                " (kan_functor_user_data_t user_data, void *generic_instance)\n{\n    ");

            kan_trivial_string_buffer_append_string (&global.generated_functions_section,
                                                     parser.current_meta_storage.explicit_shutdown_functor);
            kan_trivial_string_buffer_append_string (&global.generated_functions_section,
                                                     " (generic_instance);\n}\n\n");
        }
    }

    INCOMPATIBLE_WITH_META (parser.current_meta_storage.flags, "Structs", "flags")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.external_pointer, "Structs", "external pointer")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.has_dynamic_array_type, "Structs", "dynamic array type")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.size_field, "Structs", "size field")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.visibility_condition_field, "Structs",
                            "visibility condition field")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_visibility_condition_value, "Structs",
                            "visibility condition value")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_enum_meta, "Structs", "enum meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_enum_value_meta, "Structs", "enum value meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_struct_meta, "Structs", "struct meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_struct_field_meta, "Structs",
                            "struct field meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_function_meta, "Structs", "function meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_function_argument_meta, "Structs",
                            "function argument meta attachment")

    meta_storage_shutdown (&parser.current_meta_storage);
    meta_storage_init (&parser.current_meta_storage);
    kan_bool_t inside_union = KAN_FALSE;

    TYPE_MARKERS;
    const char *name_begin;
    const char *name_end;
    const char *array_size_begin;
    const char *array_size_end;

    while (KAN_TRUE)
    {
        parser.token = parser.cursor;
        /*!re2c
         "#" | "__pragma("
         {
             if (continue_into_potential_pragma (KAN_FALSE) == PARSE_STATUS_FAILED)
             {
                 return PARSE_STATUS_FAILED;
             }

             continue;
         }

         type @name_begin identifier @name_end separator* array_suffix? separator* ";"
         {
             if (parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT)
             {
                 kan_trivial_string_buffer_append_char_sequence (&global.declaration_section, parser.token,
                     (kan_instance_size_t) (parser.cursor - parser.token));
             }

             struct type_info_t field_type;
             ASSEMBLE_TYPE_INFO (field_type)

             if (process_struct_field (&context, name_begin, name_end, &field_type,
                 array_size_begin, array_size_end) == PARSE_STATUS_FAILED)
             {
                 return PARSE_STATUS_FAILED;
             }

             continue;
         }

         separator+
         {
             if (parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT)
             {
                 kan_trivial_string_buffer_append_char_sequence (&global.declaration_section, parser.token,
                     (kan_instance_size_t) (parser.cursor - parser.token));
             }

             continue;
         }

         "union" separator* "{"
         {
             if (inside_union)
             {
                 fprintf (stderr, "[%s:%lu:%lu] Caught union inside union, which is not supported.\n",
                     parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                     (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
                 return PARSE_STATUS_FAILED;
             }

             if (parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT)
             {
                 kan_trivial_string_buffer_append_char_sequence (&global.declaration_section, parser.token,
                     (kan_instance_size_t) (parser.cursor - parser.token));
             }

             inside_union = KAN_TRUE;
             continue;
         }

         "}" separator* ("__attribute((aligned(" [0-9]+ ")))"separator*)? ";"?
         {
             // We do not require semicolon as, due to how MSVC works, it might be after line directive.
             if (parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT)
             {
                 kan_trivial_string_buffer_append_char_sequence (&global.declaration_section, parser.token,
                     (kan_instance_size_t) (parser.cursor - parser.token));
             }

             if (inside_union)
             {
                 inside_union = KAN_FALSE;
                 continue;
             }

             if (parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT)
             {
                 kan_trivial_string_buffer_append_string (&global.declaration_section, ";\n");
             }

             finish_struct_generation (&context);
             return PARSE_STATUS_IN_PROGRESS;
         }

         *
         {
             fprintf (stderr, "[%s:%lu:%lu] Unknown expression. Expected struct field.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
             return PARSE_STATUS_FAILED;
         }

         $
         {
             fprintf (stderr, "[%s:%lu:%lu] Reached end while reading struct fields.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
             return PARSE_STATUS_FAILED;
         }
         */
    }
}

struct function_reflection_context_t
{
    kan_bool_t reflected;
    kan_instance_size_t reflected_arguments_count;
    char *name;
    struct type_info_t first_argument_type;
};

static inline kan_bool_t is_lifetime_functor (struct function_reflection_context_t *context)
{
    if (context->reflected_arguments_count != 1u || context->first_argument_type.group != TYPE_INFO_GROUP_STRUCT ||
        context->first_argument_type.pointer_level != 1u || context->first_argument_type.is_const)
    {
        return KAN_FALSE;
    }

    kan_instance_size_t prefix_length = (kan_instance_size_t) strlen (context->first_argument_type.name);
    if (prefix_length > 2u && context->first_argument_type.name[prefix_length - 2u] == '_' &&
        context->first_argument_type.name[prefix_length - 1u] == 't')
    {
        prefix_length -= 2u;
    }

    const kan_instance_size_t function_name_length = (kan_instance_size_t) strlen (context->name);
    if (function_name_length == prefix_length + 5u)
    {
        return strncmp (context->name, context->first_argument_type.name, prefix_length) == 0 &&
               strncmp (context->name + prefix_length, "_init", 5u) == 0;
    }
    else if (function_name_length == prefix_length + 9u)
    {
        return strncmp (context->name, context->first_argument_type.name, prefix_length) == 0 &&
               strncmp (context->name + prefix_length, "_shutdown", 9u) == 0;
    }

    return KAN_FALSE;
}

static inline void function_argument_bootstrap_archetype_commons (struct type_info_t *type, const char *indentation)
{
    if (type->pointer_level == 0u)
    {
        switch (type->group)
        {
        case TYPE_INFO_GROUP_VALUE:
            if (type->name == interned.type_interned_string)
            {
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, indentation);
                kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                         ".archetype = KAN_REFLECTION_ARCHETYPE_INTERNED_STRING,\n");
            }
            else if (type->name == interned.type_patch)
            {
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, indentation);
                kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                         ".archetype = KAN_REFLECTION_ARCHETYPE_PATCH,\n");
            }
            else
            {
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, indentation);
                kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                         ".archetype = ARCHETYPE_SELECTION_HELPER_GENERIC (*(");
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, type->name);
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, " *) NULL),\n");
            }

            break;

        case TYPE_INFO_GROUP_ENUM:
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, indentation);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     ".archetype = KAN_REFLECTION_ARCHETYPE_ENUM,\n");

            kan_trivial_string_buffer_append_string (&global.bootstrap_section, indentation);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     ".archetype_enum = {.type_name = kan_string_intern (\"");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, type->name);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, "\")},\n");
            break;

        case TYPE_INFO_GROUP_STRUCT:
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, indentation);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     ".archetype = KAN_REFLECTION_ARCHETYPE_STRUCT,\n");

            kan_trivial_string_buffer_append_string (&global.bootstrap_section, indentation);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     ".archetype_struct = {.type_name = kan_string_intern (\"");
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, type->name);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, "\")},\n");
            break;
        }
    }
    else
    {
        switch (type->group)
        {
        case TYPE_INFO_GROUP_VALUE:
            if (type->name == interned.type_char && type->pointer_level == 1u)
            {
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, indentation);
                kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                         ".archetype = KAN_REFLECTION_ARCHETYPE_STRING_POINTER,\n");
            }
            else
            {
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, indentation);
                kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                         ".archetype = KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER,\n");
            }

            break;

        case TYPE_INFO_GROUP_ENUM:
            kan_trivial_string_buffer_append_string (&global.bootstrap_section, indentation);
            kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                     ".archetype = KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER,\n");
            break;

        case TYPE_INFO_GROUP_STRUCT:
            if (type->pointer_level == 1u)
            {
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, indentation);
                kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                         ".archetype = KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER,\n");

                kan_trivial_string_buffer_append_string (&global.bootstrap_section, indentation);
                kan_trivial_string_buffer_append_string (
                    &global.bootstrap_section, ".archetype_struct_pointer = {.type_name = kan_string_intern (\"");
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, type->name);
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, "\")},\n");
            }
            else
            {
                kan_trivial_string_buffer_append_string (&global.bootstrap_section, indentation);
                kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                         ".archetype = KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER,\n");
            }

            break;
        }
    }
}

static inline void finish_function_generation (struct function_reflection_context_t *context,
                                               struct type_info_t *return_type_info)
{
    if (!context->reflected)
    {
        return;
    }

    kan_trivial_string_buffer_append_string (&global.generated_functions_section, "};\n#endif\n\n");
    kan_trivial_string_buffer_append_string (&global.generated_functions_section, "static void call_functor_");
    kan_trivial_string_buffer_append_string (&global.generated_functions_section, context->name);
    kan_trivial_string_buffer_append_string (&global.generated_functions_section,
                                             " (kan_functor_user_data_t user_data, void "
                                             "*return_address, void *arguments_address)\n{\n    ");

    if (return_type_info->name != interned.type_void || return_type_info->pointer_level > 0u)
    {
        kan_trivial_string_buffer_append_string (&global.generated_functions_section, "*(");
        kan_trivial_string_buffer_append_string (&global.generated_functions_section, context->name);
        kan_trivial_string_buffer_append_string (&global.generated_functions_section,
                                                 "_return_type *) return_address = ");
    }

    kan_trivial_string_buffer_append_string (&global.generated_functions_section, context->name);
    kan_trivial_string_buffer_append_string (&global.generated_functions_section, " (\n");

    for (kan_loop_size_t argument_index = 0u; argument_index < context->reflected_arguments_count; ++argument_index)
    {
        kan_trivial_string_buffer_append_string (&global.generated_functions_section, "        ((struct ");
        kan_trivial_string_buffer_append_string (&global.generated_functions_section, context->name);
        kan_trivial_string_buffer_append_string (&global.generated_functions_section,
                                                 "_call_arguments_t *) arguments_address)->_");
        kan_trivial_string_buffer_append_unsigned_long (&global.generated_functions_section,
                                                        (unsigned long) argument_index);

        if (argument_index + 1u != context->reflected_arguments_count)
        {
            kan_trivial_string_buffer_append_string (&global.generated_functions_section, ",\n");
        }
    }

    kan_trivial_string_buffer_append_string (&global.generated_functions_section, ");\n}\n\n");
    if (is_lifetime_functor (context))
    {
        kan_trivial_string_buffer_append_string (&global.generation_control_section, "#define ");
        kan_trivial_string_buffer_append_string (&global.generation_control_section, context->name);
        kan_trivial_string_buffer_append_string (&global.generation_control_section,
                                                 "_lifetime_functor lifetime_functor_");
        kan_trivial_string_buffer_append_string (&global.generation_control_section, context->name);
        kan_trivial_string_buffer_append_string (&global.generation_control_section, "\n");

        kan_trivial_string_buffer_append_string (&global.generated_functions_section, "static void lifetime_functor_");
        kan_trivial_string_buffer_append_string (&global.generated_functions_section, context->name);
        kan_trivial_string_buffer_append_string (
            &global.generated_functions_section,
            " (kan_functor_user_data_t user_data, void *generic_instance)\n{\n    ");

        kan_trivial_string_buffer_append_string (&global.generated_functions_section, context->name);
        kan_trivial_string_buffer_append_string (&global.generated_functions_section, " (generic_instance);\n}\n\n");
    }

    if (context->reflected_arguments_count > 0u)
    {
        kan_trivial_string_buffer_append_string (&global.generated_symbols_section,
                                                 "static struct kan_reflection_argument_t reflection_");
        kan_trivial_string_buffer_append_string (&global.generated_symbols_section, context->name);
        kan_trivial_string_buffer_append_string (&global.generated_symbols_section, "_arguments[");
        kan_trivial_string_buffer_append_unsigned_long (&global.generated_symbols_section,
                                                        (unsigned long) context->reflected_arguments_count);
        kan_trivial_string_buffer_append_string (&global.generated_symbols_section, "u];\n");
    }

    kan_trivial_string_buffer_append_string (&global.generated_symbols_section,
                                             "static struct kan_reflection_function_t reflection_");
    kan_trivial_string_buffer_append_string (&global.generated_symbols_section, context->name);
    kan_trivial_string_buffer_append_string (&global.generated_symbols_section, "_data;\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "    reflection_");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                             "_data = (struct kan_reflection_function_t) {\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .name = kan_string_intern (\"");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "\"),\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .call = call_functor_");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, ",\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .call_user_data = 0u,\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .return_type = {\n");
    if (return_type_info->name != interned.type_void || return_type_info->pointer_level > 0u)
    {
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .size = sizeof (");
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_return_type");
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, "),\n");
        function_argument_bootstrap_archetype_commons (return_type_info, "            ");
    }
    else
    {
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, "            .size = 0u,\n");
        kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                                 "            .archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT,\n");
    }

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        },\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .arguments_count = ");
    kan_trivial_string_buffer_append_unsigned_long (&global.bootstrap_section,
                                                    (unsigned long) context->reflected_arguments_count);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "u,\n");

    if (context->reflected_arguments_count > 0u)
    {
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .arguments = reflection_");
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_arguments,\n");
    }
    else
    {
        kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .arguments = NULL,\n");
    }

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "    };\n\n");

    kan_trivial_string_buffer_append_string (
        &global.registrar_section, "    success = kan_reflection_registry_add_function (registry, &reflection_");
    kan_trivial_string_buffer_append_string (&global.registrar_section, context->name);
    kan_trivial_string_buffer_append_string (&global.registrar_section, "_data);\n");
    kan_trivial_string_buffer_append_string (&global.registrar_section, "    KAN_ASSERT (success)\n");
}

static inline enum parse_status_t process_function_argument (struct function_reflection_context_t *context,
                                                             const char *name_begin,
                                                             const char *name_end,
                                                             struct type_info_t *type,
                                                             const char *argument_type_begin,
                                                             const char *argument_type_end)
{
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.export, "Function arguments", "export")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.flags, "Function arguments", "flags")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.ignore, "Function arguments", "ignore")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.external_pointer, "Function arguments", "external pointer")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.has_dynamic_array_type, "Function arguments",
                            "dynamic array type")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.explicit_init_functor, "Function arguments",
                            "explicit init functor")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.explicit_shutdown_functor, "Function arguments",
                            "explicit shutdown functor")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.size_field, "Function arguments", "size field")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.visibility_condition_field, "Function arguments",
                            "visibility condition field")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_visibility_condition_value, "Function arguments",
                            "visibility condition value")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_enum_meta, "Function arguments", "enum meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_enum_value_meta, "Function arguments",
                            "enum value meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_struct_meta, "Function arguments",
                            "struct meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_struct_field_meta, "Function arguments",
                            "struct field meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_function_meta, "Function arguments",
                            "function meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_function_argument_meta, "Function arguments",
                            "function argument meta attachment")

    if (!context->reflected)
    {
        meta_storage_shutdown (&parser.current_meta_storage);
        meta_storage_init (&parser.current_meta_storage);
        return PARSE_STATUS_IN_PROGRESS;
    }

    kan_trivial_string_buffer_append_string (&global.generated_functions_section, "    ");
    kan_trivial_string_buffer_append_char_sequence (&global.generated_functions_section, argument_type_begin,
                                                    (kan_instance_size_t) (argument_type_end - argument_type_begin));
    kan_trivial_string_buffer_append_string (&global.generated_functions_section, "_");
    kan_trivial_string_buffer_append_unsigned_long (&global.generated_functions_section,
                                                    (unsigned long) context->reflected_arguments_count);
    kan_trivial_string_buffer_append_string (&global.generated_functions_section, ";\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "    reflection_");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, context->name);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "_arguments[");
    kan_trivial_string_buffer_append_unsigned_long (&global.bootstrap_section,
                                                    (unsigned long) context->reflected_arguments_count);
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "u] = (struct kan_reflection_argument_t) {\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .name = kan_string_intern (\"");
    kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, name_begin,
                                                    (kan_instance_size_t) (name_end - name_begin));
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "\"),\n");

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        .size = sizeof (");
    kan_trivial_string_buffer_append_char_sequence (&global.bootstrap_section, argument_type_begin,
                                                    (kan_instance_size_t) (argument_type_end - argument_type_begin));
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "),\n");

    function_argument_bootstrap_archetype_commons (type, "        ");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "    };\n\n");

    if (context->reflected_arguments_count == 0u)
    {
        context->first_argument_type = *type;

        kan_trivial_string_buffer_append_string (&global.generation_control_section, "#define ");
        kan_trivial_string_buffer_append_string (&global.generation_control_section, context->name);
        kan_trivial_string_buffer_append_string (&global.generation_control_section, "_has_arguments\n");
    }

    ++context->reflected_arguments_count;
    meta_storage_shutdown (&parser.current_meta_storage);
    meta_storage_init (&parser.current_meta_storage);
    return PARSE_STATUS_IN_PROGRESS;
}

static enum parse_status_t parse_function_declaration (struct type_info_t *return_type_info,
                                                       const char *declaration_name_begin,
                                                       const char *declaration_name_end,
                                                       const char *whole_return_type_begin,
                                                       const char *whole_return_type_end,
                                                       const char *full_declaration_begin,
                                                       const char *full_declaration_end)
{
    struct function_reflection_context_t context = {
        .reflected = parser.current_meta_storage.export,
        .reflected_arguments_count = 0u,
        .name = NULL,
    };

    if (context.reflected)
    {
        context.name = kan_stack_group_allocator_allocate (
            &global.persistent_allocator, 1u + (declaration_name_end - declaration_name_begin), _Alignof (char));
        memcpy (context.name, declaration_name_begin, declaration_name_end - declaration_name_begin);
        context.name[declaration_name_end - declaration_name_begin] = '\0';

        kan_trivial_string_buffer_append_string (&global.generation_control_section, "#define ");
        kan_trivial_string_buffer_append_string (&global.generation_control_section, context.name);
        kan_trivial_string_buffer_append_string (&global.generation_control_section, "_return_type ");
        kan_trivial_string_buffer_append_char_sequence (
            &global.generation_control_section, whole_return_type_begin,
            (kan_instance_size_t) (whole_return_type_end - whole_return_type_begin));
        kan_trivial_string_buffer_append_string (&global.generation_control_section, "\n");

        kan_trivial_string_buffer_append_string (&global.generated_functions_section, "#if defined(");
        kan_trivial_string_buffer_append_string (&global.generated_functions_section, context.name);
        kan_trivial_string_buffer_append_string (&global.generated_functions_section, "_has_arguments)\n");
        kan_trivial_string_buffer_append_string (&global.generated_functions_section, "struct ");
        kan_trivial_string_buffer_append_string (&global.generated_functions_section, context.name);
        kan_trivial_string_buffer_append_string (&global.generated_functions_section, "_call_arguments_t\n{\n");
    }

    if (context.reflected && parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT)
    {
        kan_trivial_string_buffer_append_string (&global.declaration_section, "EXPORT_THIS ");
        kan_trivial_string_buffer_append_char_sequence (
            &global.declaration_section, full_declaration_begin,
            (kan_instance_size_t) (full_declaration_end - full_declaration_begin));
    }

    INCOMPATIBLE_WITH_META (parser.current_meta_storage.flags, "Functions", "flags")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.external_pointer, "Functions", "external pointer")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.has_dynamic_array_type, "Functions", "dynamic array type")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.explicit_init_functor, "Functions", "explicit init functor")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.explicit_shutdown_functor, "Functions",
                            "explicit shutdown functor")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.size_field, "Functions", "size field")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.visibility_condition_field, "Functions",
                            "visibility condition field")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_visibility_condition_value, "Functions",
                            "visibility condition value")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_enum_meta, "Functions", "enum meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_enum_value_meta, "Functions",
                            "enum value meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_struct_meta, "Functions", "struct meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_struct_field_meta, "Functions",
                            "struct field meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_function_meta, "Functions", "function meta attachment")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_function_argument_meta, "Functions",
                            "function argument meta attachment")

    meta_storage_shutdown (&parser.current_meta_storage);
    meta_storage_init (&parser.current_meta_storage);

    TYPE_MARKERS;
    const char *name_begin;
    const char *name_end;

    const char *argument_type_begin;
    const char *argument_type_end;

    kan_bool_t had_any_arguments = KAN_FALSE;
    const char *void_marker;

    while (KAN_TRUE)
    {
        parser.token = parser.cursor;
        /*!re2c
         "#" | "__pragma("
         {
             if (continue_into_potential_pragma (KAN_FALSE) == PARSE_STATUS_FAILED)
             {
                 return PARSE_STATUS_FAILED;
             }

             continue;
         }

         @argument_type_begin type @argument_type_end (separator* "__"? "restrict" separator+)?
         @name_begin identifier @name_end
         {
             if (context.reflected && parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT)
             {
                 kan_trivial_string_buffer_append_char_sequence (&global.declaration_section, parser.token,
                     (kan_instance_size_t) (parser.cursor - parser.token));
             }

             struct type_info_t argument_type;
             ASSEMBLE_TYPE_INFO (argument_type)

             if (process_function_argument (&context, name_begin, name_end, &argument_type,
                 argument_type_begin, argument_type_end) == PARSE_STATUS_FAILED)
             {
                 return PARSE_STATUS_FAILED;
             }

             had_any_arguments = KAN_TRUE;
             continue;
         }

         "," | separator+
         {
             if (context.reflected && parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT)
             {
                 kan_trivial_string_buffer_append_char_sequence (&global.declaration_section, parser.token,
                     (kan_instance_size_t) (parser.cursor - parser.token));
             }

             continue;
         }

         "..."
         {
             if (context.reflected && parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT)
             {
                 fprintf (stderr, "[%s:%lu:%lu] Encountered variadic expression which is not supported for reflected "
                     "functions.\n",
                     parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                     (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
                 return PARSE_STATUS_FAILED;
             }

             continue;
         }

         (@void_marker "void" separator*)? ")"
         {
             if (context.reflected && parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT)
             {
                 kan_trivial_string_buffer_append_char_sequence (&global.declaration_section, parser.token,
                     (kan_instance_size_t) (parser.cursor - parser.token));
                 kan_trivial_string_buffer_append_string (&global.declaration_section, ";\n");
             }

             if (void_marker && had_any_arguments)
             {
                 fprintf (stderr,
                     "[%s:%lu:%lu] Encountered void argument marker when function already has arguments.\n",
                     parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                     (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
                 return PARSE_STATUS_FAILED;
             }

             finish_function_generation (&context, return_type_info);
             return PARSE_STATUS_IN_PROGRESS;
         }

         *
         {
             fprintf (stderr, "[%s:%lu:%lu] Unknown expression. Expected function argument.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
             return PARSE_STATUS_FAILED;
         }

         $
         {
             fprintf (stderr, "[%s:%lu:%lu] Reached end while reading function arguments.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
             return PARSE_STATUS_FAILED;
         }
         */
    }
}

static enum parse_status_t process_parsed_symbol (const char *declaration_name_begin,
                                                  const char *declaration_name_end,
                                                  struct type_info_t *type,
                                                  const char *symbol_info_begin,
                                                  const char *symbol_info_end)
{
    if (parser.current_target_node->type == TARGET_FILE_TYPE_OBJECT && parser.current_meta_storage.export)
    {
        kan_trivial_string_buffer_append_string (&global.declaration_section, "extern ");
        kan_trivial_string_buffer_append_char_sequence (&global.declaration_section, symbol_info_begin,
                                                        (kan_instance_size_t) (symbol_info_end - symbol_info_begin));
        kan_trivial_string_buffer_append_string (&global.declaration_section, ";\n");
    }

    INCOMPATIBLE_WITH_META (parser.current_meta_storage.flags, "Symbols", "flags")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.external_pointer, "Symbols", "external pointer")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.has_dynamic_array_type, "Symbols", "dynamic array type")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.explicit_init_functor, "Symbols", "explicit init functor")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.explicit_shutdown_functor, "Symbols",
                            "explicit shutdown functor")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.size_field, "Symbols", "size field")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.visibility_condition_field, "Symbols",
                            "visibility condition field")
    INCOMPATIBLE_WITH_META (parser.current_meta_storage.first_visibility_condition_value, "Symbols",
                            "visibility condition value")

    if (!parser.current_meta_storage.export)
    {
        meta_storage_shutdown (&parser.current_meta_storage);
        meta_storage_init (&parser.current_meta_storage);
        return PARSE_STATUS_IN_PROGRESS;
    }

    if (type->group != TYPE_INFO_GROUP_STRUCT)
    {
        fprintf (stderr, "[%s:%lu:%lu] Found exported non-struct symbols. Only struct symbols can be exported.\n",
                 parser.current_target_node ? parser.current_target_node->path : "<unknown>",
                 (unsigned long) parser.current_target_line, (unsigned long) parser.cursor_symbol - 1u);
        return PARSE_STATUS_FAILED;
    }

    struct top_level_meta_t *top_level_meta;
    struct secondary_level_meta_t *secondary_level_meta;

#define TOP_LEVEL_META(TYPE)                                                                                           \
    top_level_meta = parser.current_meta_storage.first_##TYPE##_meta;                                                  \
    while (top_level_meta)                                                                                             \
    {                                                                                                                  \
        kan_trivial_string_buffer_append_string (&global.registrar_section, "    kan_reflection_registry_add_" #TYPE   \
                                                                            "_meta (registry, kan_string_intern (\""); \
        kan_trivial_string_buffer_append_string (&global.registrar_section, top_level_meta->top_level_name);           \
        kan_trivial_string_buffer_append_string (&global.registrar_section, "\"), kan_string_intern (\"");             \
        kan_trivial_string_buffer_append_string (&global.registrar_section, type->name);                               \
        kan_trivial_string_buffer_append_string (&global.registrar_section, "\"), &");                                 \
        kan_trivial_string_buffer_append_char_sequence (                                                               \
            &global.registrar_section, declaration_name_begin,                                                         \
            (kan_instance_size_t) (declaration_name_end - declaration_name_begin));                                    \
        kan_trivial_string_buffer_append_string (&global.registrar_section, ");\n");                                   \
        top_level_meta = top_level_meta->next;                                                                         \
    }

#define SECONDARY_LEVEL_META(TYPE_TOP, TYPE_SECONDARY)                                                                 \
    secondary_level_meta = parser.current_meta_storage.first_##TYPE_TOP##_##TYPE_SECONDARY##_meta;                     \
    while (secondary_level_meta)                                                                                       \
    {                                                                                                                  \
        kan_trivial_string_buffer_append_string (&global.registrar_section,                                            \
                                                 "    kan_reflection_registry_add_" #TYPE_TOP "_" #TYPE_SECONDARY      \
                                                 "_meta (registry, kan_string_intern (\"");                            \
        kan_trivial_string_buffer_append_string (&global.registrar_section, secondary_level_meta->top_level_name);     \
        kan_trivial_string_buffer_append_string (&global.registrar_section, "\"), kan_string_intern (\"");             \
        kan_trivial_string_buffer_append_string (&global.registrar_section,                                            \
                                                 secondary_level_meta->secondary_level_name);                          \
        kan_trivial_string_buffer_append_string (&global.registrar_section, "\"), kan_string_intern (\"");             \
        kan_trivial_string_buffer_append_string (&global.registrar_section, type->name);                               \
        kan_trivial_string_buffer_append_string (&global.registrar_section, "\"), &");                                 \
        kan_trivial_string_buffer_append_char_sequence (                                                               \
            &global.registrar_section, declaration_name_begin,                                                         \
            (kan_instance_size_t) (declaration_name_end - declaration_name_begin));                                    \
        kan_trivial_string_buffer_append_string (&global.registrar_section, ");\n");                                   \
        secondary_level_meta = secondary_level_meta->next;                                                             \
    }

    TOP_LEVEL_META (enum)
    SECONDARY_LEVEL_META (enum, value)
    TOP_LEVEL_META (struct)
    SECONDARY_LEVEL_META (struct, field)
    TOP_LEVEL_META (function)
    SECONDARY_LEVEL_META (function, argument)

#undef TOP_LEVEL_META
#undef SECONDARY_LEVEL_META
    meta_storage_shutdown (&parser.current_meta_storage);
    meta_storage_init (&parser.current_meta_storage);
    return PARSE_STATUS_IN_PROGRESS;
}

static enum parse_status_t parse_skip_until_curly_braces_close (void)
{
    size_t left_to_close = 1u;
    while (KAN_TRUE)
    {
        parser.token = parser.cursor;
        /*!re2c
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
                 return PARSE_STATUS_IN_PROGRESS;
             }

             continue;
         }

         * { continue; }
         $
         {
             fprintf (stderr, "Error. Reached end of file while waiting for curly braces to close.\n");
             return PARSE_STATUS_FAILED;
         }
        */
    }
}

static enum parse_status_t parse_skip_until_round_braces_close (void)
{
    size_t left_to_close = 1u;
    while (KAN_TRUE)
    {
        parser.token = parser.cursor;
        /*!re2c
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
                return PARSE_STATUS_IN_PROGRESS;
            }

            continue;
        }

        * { continue; }
        $
        {
            fprintf (stderr, "Error. Reached end of file while waiting for round braces to close.\n");
            return PARSE_STATUS_FAILED;
        }
        */
    }
}

static inline void print_arguments_help (void)
{
    fprintf (stderr,
             "Incorrect arguments. Expected:\n"
             "- Absolute path to product file.\n"
             "- Name of the unit for reflection registrar.\n"
             "- Absolute path to text file that lists all files to generate reflection for as lines.\n"
             "- Absolute path to text file that lists all preprocessed files to scan for data as lines.\n");
}

static const char product_head[] =
    "// Autogenerated by reflection_preprocessor tool, do not modify manually.\n"
    "#include <stddef.h>\n"
    "\n"
    "#include <kan/api_common/core_types.h>\n"
    "#include <kan/api_common/mute_warnings.h>\n"
    "#include <kan/container/interned_string.h>\n"
    "#include <kan/error/critical.h>\n"
    "#include <kan/reflection/generated_reflection.h>\n"
    "#include <kan/reflection/registry.h>\n"
    "\n"
    "#define ARCHETYPE_SELECTION_HELPER_GENERIC(EXPRESSION) \\\n"
    "    _Generic ((EXPRESSION),\\\n"
    "        int8_t: KAN_REFLECTION_ARCHETYPE_SIGNED_INT, \\\n"
    "        int16_t: KAN_REFLECTION_ARCHETYPE_SIGNED_INT, \\\n"
    "        int32_t: KAN_REFLECTION_ARCHETYPE_SIGNED_INT, \\\n"
    "        int64_t: KAN_REFLECTION_ARCHETYPE_SIGNED_INT, \\\n"
    "        uint8_t: KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT, \\\n"
    "        uint16_t: KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT, \\\n"
    "        uint32_t: KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT, \\\n"
    "        uint64_t: KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT, \\\n"
    "        float: KAN_REFLECTION_ARCHETYPE_FLOATING, \\\n"
    "        double: KAN_REFLECTION_ARCHETYPE_FLOATING, \\\n"
    "        default: KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL)\n"
    "\n"
    "#define ARCHETYPE_SELECTION_HELPER(STRUCTURE, FIELD) \\\n"
    "    ARCHETYPE_SELECTION_HELPER_GENERIC(((struct STRUCTURE *) NULL)->FIELD)\n"
    "\n"
    "#define SIZE_OF_FIELD(STRUCTURE, FIELD) sizeof (((struct STRUCTURE *) NULL)->FIELD)\n"
    "\n"
    "#if defined(_WIN32)\n"
    "#    define EXPORT_THIS __declspec(dllexport)\n"
    "#else\n"
    "#    define EXPORT_THIS\n"
    "#endif\n";

static inline void remove_trailing_special_characters (char *buffer)
{
    kan_instance_size_t length = (kan_instance_size_t) strlen (buffer);
    while (length > 0u && iscntrl (buffer[length - 1u]))
    {
        --length;
    }

    buffer[length] = '\0';
}

int main (int arguments_count, char **argument_values)
{
    if (arguments_count != 5)
    {
        print_arguments_help ();
        return RETURN_CODE_INVALID_ARGUMENTS;
    }

    arguments.product = argument_values[1u];
    arguments.unit_name = argument_values[2u];
    arguments.target_file_list = argument_values[3u];
    arguments.input_file_list = argument_values[4u];

    interned.type_void = kan_string_intern ("void");
    interned.type_char = kan_string_intern ("char");
    interned.type_interned_string = kan_string_intern ("kan_interned_string_t");
    interned.type_dynamic_array = kan_string_intern ("kan_dynamic_array_t");
    interned.type_patch = kan_string_intern ("kan_reflection_patch_t");

    int result = RETURN_CODE_SUCCESS;
    global.main_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "reflection_preprocessor");
    global.section_allocation_group = kan_allocation_group_get_child (global.main_allocation_group, "sections");
    global.persistent_allocation_group = kan_allocation_group_get_child (global.main_allocation_group, "persistent");
    global.hash_storage_allocation_group =
        kan_allocation_group_get_child (global.main_allocation_group, "hash_storage");
    global.meta_allocation_group = kan_allocation_group_get_child (global.main_allocation_group, "meta");

    kan_stack_group_allocator_init (&global.persistent_allocator, global.persistent_allocation_group,
                                    KAN_REFLECTION_PREPROCESSOR_STACK_ALLOCATOR_ITEM);
    kan_hash_storage_init (&global.target_files, global.hash_storage_allocation_group,
                           KAN_REFLECTION_PREPROCESSOR_TARGET_FILE_BUCKETS);
    kan_hash_storage_init (&global.included_files, global.hash_storage_allocation_group,
                           KAN_REFLECTION_PREPROCESSOR_INCLUDED_FILE_BUCKETS);
    meta_storage_init (&parser.current_meta_storage);

    kan_trivial_string_buffer_init (&global.declaration_section, global.section_allocation_group,
                                    KAN_REFLECTION_PREPROCESSOR_SECTION_CAPACITY);
    kan_trivial_string_buffer_append_string (
        &global.declaration_section,
        "\n// Declaration section: contains all typedefs, structs, exported functions and symbols.\n\n");

    kan_trivial_string_buffer_init (&global.generation_control_section, global.section_allocation_group,
                                    KAN_REFLECTION_PREPROCESSOR_SECTION_CAPACITY);

    kan_trivial_string_buffer_append_string (
        &global.generation_control_section,
        "\n// Generation control section: contains generated macros for code generation control.\n\n");

    kan_trivial_string_buffer_init (&global.generated_functions_section, global.section_allocation_group,
                                    KAN_REFLECTION_PREPROCESSOR_SECTION_CAPACITY);

    kan_trivial_string_buffer_append_string (
        &global.generated_functions_section,
        "\n// Generated functions section: contains generated functions and their call arguments if necessary.\n\n");

    kan_trivial_string_buffer_init (&global.generated_symbols_section, global.section_allocation_group,
                                    KAN_REFLECTION_PREPROCESSOR_SECTION_CAPACITY);

    kan_trivial_string_buffer_append_string (&global.generated_symbols_section,
                                             "\n// Generated symbols section: contains generated symbols.\n\n");

    kan_trivial_string_buffer_init (&global.bootstrap_section, global.section_allocation_group,
                                    KAN_REFLECTION_PREPROCESSOR_SECTION_CAPACITY);

    kan_trivial_string_buffer_append_string (
        &global.bootstrap_section, "\n// Boostrap section: contains logic that fills generated symbols with data.\n\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                             "static kan_bool_t bootstrap_done = KAN_FALSE;\n\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section,
                                             "static void ensure_reflection_is_ready (void)\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "{\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "    if (bootstrap_done)\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "    {\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "        return;\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "    }\n\n");

    kan_trivial_string_buffer_init (&global.registrar_section, global.section_allocation_group,
                                    KAN_REFLECTION_PREPROCESSOR_SECTION_CAPACITY);

    kan_trivial_string_buffer_append_string (
        &global.registrar_section, "\n// Registrar section: contains logic that registers reflection data.\n\n");
    kan_trivial_string_buffer_append_string (&global.registrar_section,
                                             "EXPORT_THIS void KAN_REFLECTION_UNIT_REGISTRAR_NAME (");
    kan_trivial_string_buffer_append_string (&global.registrar_section, arguments.unit_name);
    kan_trivial_string_buffer_append_string (&global.registrar_section, ") (kan_reflection_registry_t registry)\n");
    kan_trivial_string_buffer_append_string (&global.registrar_section, "{\n");
    kan_trivial_string_buffer_append_string (&global.registrar_section, "    ensure_reflection_is_ready ();\n");
    kan_trivial_string_buffer_append_string (&global.registrar_section, "    KAN_MUTE_UNUSED_WARNINGS_BEGIN\n");
    kan_trivial_string_buffer_append_string (&global.registrar_section, "    kan_bool_t success;\n\n");

    // We use standard C file API for reading file lists as its just much better suited for this task than streams.

    FILE *target_list_file = fopen (arguments.target_file_list, "r");
    if (target_list_file)
    {
        char buffer[KAN_FILE_SYSTEM_MAX_PATH_LENGTH + 1u];
        while (fgets (buffer, sizeof (buffer), target_list_file))
        {
            remove_trailing_special_characters (buffer);
            struct target_file_node_t *created = create_target_file_node (buffer);

            if (created->type == TARGET_FILE_TYPE_UNKNOWN)
            {
                fprintf (stderr, "Unable to detect target file\"%s\" type.\n", buffer);
                result = RETURN_CODE_TARGET_LIST_LOAD_FAILED;
            }
        }

        fclose (target_list_file);
    }
    else
    {
        fprintf (stderr, "Failed to open target list file \"%s\".\n", arguments.target_file_list);
        result = RETURN_CODE_TARGET_LIST_LOAD_FAILED;
    }

    if (result == RETURN_CODE_SUCCESS)
    {
        global.current_input_index = 0u;
        FILE *input_list_file = fopen (arguments.input_file_list, "r");

        if (input_list_file)
        {
            char buffer[KAN_FILE_SYSTEM_MAX_PATH_LENGTH + 1u];
            while (fgets (buffer, sizeof (buffer), input_list_file))
            {
                remove_trailing_special_characters (buffer);
                struct kan_stream_t *input_stream = kan_direct_file_stream_open_for_read (buffer, KAN_FALSE);

                if (!input_stream)
                {
                    fprintf (stderr, "Failed to open input file \"%s\".\n", buffer);
                    result = RETURN_CODE_PARSE_FAILED;
                    break;
                }

                // Buffering is done on re2c side, we do not need to create additional buffer here.
                reset_parser_state (input_stream);
                enum parse_status_t status;

                while ((status = parse_main ()) == PARSE_STATUS_IN_PROGRESS)
                {
                }

                input_stream->operations->close (input_stream);
                if (status != PARSE_STATUS_FINISHED)
                {
                    fprintf (stderr,
                             "Stopping due to errors encountered while parsing. Parser stopped at [%s:%lu:%lu].\n",
                             buffer, (unsigned long) parser.cursor_line, (unsigned long) parser.cursor_symbol - 1u);
                    result = RETURN_CODE_PARSE_FAILED;
                    break;
                }

                ++global.current_input_index;
            }

            fclose (input_list_file);
        }
        else
        {
            fprintf (stderr, "Failed to open input list file \"%s\".\n", arguments.input_file_list);
            result = RETURN_CODE_INPUT_LIST_LOAD_FAILED;
        }
    }

    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "\n    bootstrap_done = KAN_TRUE;\n");
    kan_trivial_string_buffer_append_string (&global.bootstrap_section, "}\n");

    kan_trivial_string_buffer_append_string (&global.registrar_section, "\n    KAN_MUTE_UNUSED_WARNINGS_END\n");
    kan_trivial_string_buffer_append_string (&global.registrar_section, "}\n");

    if (result == RETURN_CODE_SUCCESS)
    {
        // We use binary mode for writing large chunk of text as text mode glitches on Windows for some reason.
        struct kan_stream_t *write_stream = kan_direct_file_stream_open_for_write (arguments.product, KAN_TRUE);

        if (write_stream)
        {
            write_stream =
                kan_random_access_stream_buffer_open_for_write (write_stream, KAN_REFLECTION_PREPROCESSOR_IO_BUFFER);

            if (write_stream->operations->write (write_stream, sizeof (product_head) - 1u, product_head) !=
                sizeof (product_head) - 1u)
            {
                fprintf (stderr, "Error while writing product head.\n");
                result = RETURN_CODE_WRITE_FAILED;
            }

            if (write_stream->operations->write (write_stream, global.declaration_section.size,
                                                 global.declaration_section.buffer) != global.declaration_section.size)
            {
                fprintf (stderr, "Error while writing declaration section.\n");
                result = RETURN_CODE_WRITE_FAILED;
            }

            if (write_stream->operations->write (write_stream, global.generation_control_section.size,
                                                 global.generation_control_section.buffer) !=
                global.generation_control_section.size)
            {
                fprintf (stderr, "Error while writing generated types section.\n");
                result = RETURN_CODE_WRITE_FAILED;
            }

            if (write_stream->operations->write (write_stream, global.generated_functions_section.size,
                                                 global.generated_functions_section.buffer) !=
                global.generated_functions_section.size)
            {
                fprintf (stderr, "Error while writing generated functions section.\n");
                result = RETURN_CODE_WRITE_FAILED;
            }

            if (write_stream->operations->write (write_stream, global.generated_symbols_section.size,
                                                 global.generated_symbols_section.buffer) !=
                global.generated_symbols_section.size)
            {
                fprintf (stderr, "Error while writing generated symbols section.\n");
                result = RETURN_CODE_WRITE_FAILED;
            }

            if (write_stream->operations->write (write_stream, global.bootstrap_section.size,
                                                 global.bootstrap_section.buffer) != global.bootstrap_section.size)
            {
                fprintf (stderr, "Error while writing bootstrap section.\n");
                result = RETURN_CODE_WRITE_FAILED;
            }

            if (write_stream->operations->write (write_stream, global.registrar_section.size,
                                                 global.registrar_section.buffer) != global.registrar_section.size)
            {
                fprintf (stderr, "Error while writing registrar section.\n");
                result = RETURN_CODE_WRITE_FAILED;
            }

            write_stream->operations->close (write_stream);
        }
        else
        {
            fprintf (stderr, "Failed to open product file for write.\n");
            result = RETURN_CODE_WRITE_FAILED;
        }
    }

    kan_trivial_string_buffer_shutdown (&global.declaration_section);
    kan_trivial_string_buffer_shutdown (&global.generation_control_section);
    kan_trivial_string_buffer_shutdown (&global.generated_functions_section);
    kan_trivial_string_buffer_shutdown (&global.generated_symbols_section);
    kan_trivial_string_buffer_shutdown (&global.bootstrap_section);
    kan_trivial_string_buffer_shutdown (&global.registrar_section);

    kan_hash_storage_shutdown (&global.included_files);
    kan_hash_storage_shutdown (&global.target_files);
    kan_stack_group_allocator_shutdown (&global.persistent_allocator);
    return result;
}
