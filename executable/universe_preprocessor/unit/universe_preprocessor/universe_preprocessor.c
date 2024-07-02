#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/min_max.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/container/trivial_string_buffer.h>
#include <kan/error/critical.h>
#include <kan/file_system/stream.h>
#include <kan/memory/allocation.h>
#include <kan/stream/random_access_stream_buffer.h>

#define RETURN_CODE_SUCCESS 0
#define RETURN_CODE_INVALID_ARGUMENTS (-1)
#define RETURN_CODE_UNABLE_TO_OPEN_INPUT (-2)
#define RETURN_CODE_PARSE_FAILED (-3)
#define RETURN_CODE_PROCESS_FAILED (-4)
#define RETURN_CODE_UNABLE_TO_OPEN_OUTPUT (-5)

#define INPUT_ERROR_REFIL_AFTER_END_OF_FILE 1
#define INPUT_ERROR_LEXEME_OVERFLOW 2

// Global state.
static struct
{
    const char *input_file_path;
    const char *output_file_path;
} arguments;

#define INPUT_BUFFER_SIZE 131072u // 128 kilobytes

struct tags_t
{
    /*!stags:re2c format = 'const char *@@;';*/
};

enum parse_response_t
{
    PARSE_RESPONSE_FINISHED = 0u,
    PARSE_RESPONSE_FAILED,
    PARSE_RESPONSE_BLOCK_PROCESSED,
};

static struct
{
    struct kan_stream_t *input_stream;
    struct kan_stream_t *output_stream;

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

enum singleton_access_type_t
{
    SINGLETON_ACCESS_TYPE_READ = 0u,
    SINGLETON_ACCESS_TYPE_WRITE,
};

enum indexed_access_type_t
{
    INDEXED_ACCESS_TYPE_READ = 0u,
    INDEXED_ACCESS_TYPE_UPDATE,
    INDEXED_ACCESS_TYPE_DELETE,
    INDEXED_ACCESS_TYPE_WRITE,
};

struct scanned_indexed_field_query_t
{
    kan_interned_string_t type;
    char field_path[KAN_UNIVERSE_PREPROCESSOR_TARGET_PATH_MAX_LENGTH];
};

struct scanned_signal_query_t
{
    kan_interned_string_t type;
    char field_path[KAN_UNIVERSE_PREPROCESSOR_TARGET_PATH_MAX_LENGTH];
    char signal_value[KAN_UNIVERSE_PREPROCESSOR_SIGNAL_VALUE_MAX_LENGTH];
};

struct scanned_state_t
{
    kan_interned_string_t name;

    /// \meta reflection_dynamic_array_type = "kan_interned_string"
    struct kan_dynamic_array_t singleton_read_queries;

    /// \meta reflection_dynamic_array_type = "kan_interned_string"
    struct kan_dynamic_array_t singleton_write_queries;

    /// \meta reflection_dynamic_array_type = "kan_interned_string"
    struct kan_dynamic_array_t indexed_insert_queries;

    /// \meta reflection_dynamic_array_type = "kan_interned_string"
    struct kan_dynamic_array_t indexed_sequence_read_queries;

    /// \meta reflection_dynamic_array_type = "kan_interned_string"
    struct kan_dynamic_array_t indexed_sequence_update_queries;

    /// \meta reflection_dynamic_array_type = "kan_interned_string"
    struct kan_dynamic_array_t indexed_sequence_delete_queries;

    /// \meta reflection_dynamic_array_type = "kan_interned_string"
    struct kan_dynamic_array_t indexed_sequence_write_queries;

    /// \meta reflection_dynamic_array_type = "struct scanned_indexed_field_query_t"
    struct kan_dynamic_array_t indexed_value_read_queries;

    /// \meta reflection_dynamic_array_type = "struct scanned_indexed_field_query_t"
    struct kan_dynamic_array_t indexed_value_update_queries;

    /// \meta reflection_dynamic_array_type = "struct scanned_indexed_field_query_t"
    struct kan_dynamic_array_t indexed_value_delete_queries;

    /// \meta reflection_dynamic_array_type = "struct scanned_indexed_field_query_t"
    struct kan_dynamic_array_t indexed_value_write_queries;

    /// \meta reflection_dynamic_array_type = "struct scanned_signal_query_t"
    struct kan_dynamic_array_t indexed_signal_read_queries;

    /// \meta reflection_dynamic_array_type = "struct scanned_signal_query_t"
    struct kan_dynamic_array_t indexed_signal_update_queries;

    /// \meta reflection_dynamic_array_type = "struct scanned_signal_query_t"
    struct kan_dynamic_array_t indexed_signal_delete_queries;

    /// \meta reflection_dynamic_array_type = "struct scanned_signal_query_t"
    struct kan_dynamic_array_t indexed_signal_write_queries;

    /// \meta reflection_dynamic_array_type = "struct scanned_indexed_field_query_t"
    struct kan_dynamic_array_t indexed_interval_read_queries;

    /// \meta reflection_dynamic_array_type = "struct scanned_indexed_field_query_t"
    struct kan_dynamic_array_t indexed_interval_update_queries;

    /// \meta reflection_dynamic_array_type = "struct scanned_indexed_field_query_t"
    struct kan_dynamic_array_t indexed_interval_delete_queries;

    /// \meta reflection_dynamic_array_type = "struct scanned_indexed_field_query_t"
    struct kan_dynamic_array_t indexed_interval_write_queries;

    /// \meta reflection_dynamic_array_type = "kan_interned_string"
    struct kan_dynamic_array_t event_insert_queries;

    /// \meta reflection_dynamic_array_type = "kan_interned_string"
    struct kan_dynamic_array_t event_fetch_queries;
};

struct
{
    kan_allocation_group_t allocation_group;
    uint64_t scan_bound_state_index;

    /// \meta reflection_dynamic_array_type = "struct scanned_state_t"
    struct kan_dynamic_array_t states;
} scan;

enum process_query_type_t
{
    PROCESS_QUERY_TYPE_SINGLETON_READ = 0u,
    PROCESS_QUERY_TYPE_SINGLETON_WRITE,
    PROCESS_QUERY_TYPE_INDEXED_INSERT,
    PROCESS_QUERY_TYPE_SEQUENCE_READ,
    PROCESS_QUERY_TYPE_SEQUENCE_UPDATE,
    PROCESS_QUERY_TYPE_SEQUENCE_DELETE,
    PROCESS_QUERY_TYPE_SEQUENCE_WRITE,
    PROCESS_QUERY_TYPE_VALUE_READ,
    PROCESS_QUERY_TYPE_VALUE_UPDATE,
    PROCESS_QUERY_TYPE_VALUE_DELETE,
    PROCESS_QUERY_TYPE_VALUE_WRITE,
    PROCESS_QUERY_TYPE_SIGNAL_READ,
    PROCESS_QUERY_TYPE_SIGNAL_UPDATE,
    PROCESS_QUERY_TYPE_SIGNAL_DELETE,
    PROCESS_QUERY_TYPE_SIGNAL_WRITE,
    PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_READ,
    PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_UPDATE,
    PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_DELETE,
    PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_WRITE,
    PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_READ,
    PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_UPDATE,
    PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_DELETE,
    PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_WRITE,
    PROCESS_QUERY_TYPE_EVENT_INSERT,
    PROCESS_QUERY_TYPE_EVENT_FETCH,
};

struct process_query_stack_node_t
{
    struct process_query_stack_node_t *previous;
    uint64_t blocks;
    kan_interned_string_t name;
    enum process_query_type_t query_type;
};

struct
{
    kan_allocation_group_t allocation_group;
    uint64_t output_file_line;

    struct scanned_state_t *bound_state;
    char bound_state_path[KAN_UNIVERSE_PREPROCESSOR_STATE_PATH_MAX_LENGTH];

    uint64_t blocks;
    struct process_query_stack_node_t *stack_top;

} process = {
    .output_file_line = 1u,
    .bound_state = NULL,
    .blocks = 0u,
    .stack_top = NULL,
};

// IO

static void io_reset (void)
{
    io.input_buffer[0u] = '\0';
    io.limit = io.input_buffer + INPUT_BUFFER_SIZE - 1u;
    io.cursor = io.input_buffer + INPUT_BUFFER_SIZE - 1u;
    io.marker = io.input_buffer + INPUT_BUFFER_SIZE - 1u;
    io.token = io.input_buffer + INPUT_BUFFER_SIZE - 1u;
    io.end_of_input_reached = KAN_FALSE;
    io.cursor_line = 1u;
    io.cursor_symbol = 1u;
    io.marker_line = 1u;
    io.marker_symbol = 1u;
}

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

// Scan phase processing

static void scanned_state_init (struct scanned_state_t *state)
{
    kan_dynamic_array_init (&state->singleton_read_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t), scan.allocation_group);
    kan_dynamic_array_init (&state->singleton_write_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t), scan.allocation_group);

    kan_dynamic_array_init (&state->indexed_insert_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t), scan.allocation_group);
    kan_dynamic_array_init (&state->indexed_sequence_read_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t), scan.allocation_group);
    kan_dynamic_array_init (&state->indexed_sequence_update_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t), scan.allocation_group);
    kan_dynamic_array_init (&state->indexed_sequence_delete_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t), scan.allocation_group);
    kan_dynamic_array_init (&state->indexed_sequence_write_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t), scan.allocation_group);

    kan_dynamic_array_init (&state->indexed_value_read_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (struct scanned_indexed_field_query_t),
                            _Alignof (struct scanned_indexed_field_query_t), scan.allocation_group);
    kan_dynamic_array_init (&state->indexed_value_update_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (struct scanned_indexed_field_query_t),
                            _Alignof (struct scanned_indexed_field_query_t), scan.allocation_group);
    kan_dynamic_array_init (&state->indexed_value_delete_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (struct scanned_indexed_field_query_t),
                            _Alignof (struct scanned_indexed_field_query_t), scan.allocation_group);
    kan_dynamic_array_init (&state->indexed_value_write_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (struct scanned_indexed_field_query_t),
                            _Alignof (struct scanned_indexed_field_query_t), scan.allocation_group);

    kan_dynamic_array_init (&state->indexed_signal_read_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (struct scanned_signal_query_t), _Alignof (struct scanned_signal_query_t),
                            scan.allocation_group);
    kan_dynamic_array_init (&state->indexed_signal_update_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (struct scanned_signal_query_t), _Alignof (struct scanned_signal_query_t),
                            scan.allocation_group);
    kan_dynamic_array_init (&state->indexed_signal_delete_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (struct scanned_signal_query_t), _Alignof (struct scanned_signal_query_t),
                            scan.allocation_group);
    kan_dynamic_array_init (&state->indexed_signal_write_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (struct scanned_signal_query_t), _Alignof (struct scanned_signal_query_t),
                            scan.allocation_group);

    kan_dynamic_array_init (&state->indexed_interval_read_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (struct scanned_indexed_field_query_t),
                            _Alignof (struct scanned_indexed_field_query_t), scan.allocation_group);
    kan_dynamic_array_init (&state->indexed_interval_update_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (struct scanned_indexed_field_query_t),
                            _Alignof (struct scanned_indexed_field_query_t), scan.allocation_group);
    kan_dynamic_array_init (&state->indexed_interval_delete_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (struct scanned_indexed_field_query_t),
                            _Alignof (struct scanned_indexed_field_query_t), scan.allocation_group);
    kan_dynamic_array_init (&state->indexed_interval_write_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (struct scanned_indexed_field_query_t),
                            _Alignof (struct scanned_indexed_field_query_t), scan.allocation_group);

    kan_dynamic_array_init (&state->event_insert_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t), scan.allocation_group);
    kan_dynamic_array_init (&state->event_fetch_queries, KAN_UNIVERSE_PREPROCESSOR_QUERY_INITIAL_SLOTS,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t), scan.allocation_group);
}

static void scanned_state_shutdown (struct scanned_state_t *state)
{
    kan_dynamic_array_shutdown (&state->singleton_read_queries);
    kan_dynamic_array_shutdown (&state->singleton_write_queries);
    kan_dynamic_array_shutdown (&state->indexed_insert_queries);
    kan_dynamic_array_shutdown (&state->indexed_sequence_read_queries);
    kan_dynamic_array_shutdown (&state->indexed_sequence_update_queries);
    kan_dynamic_array_shutdown (&state->indexed_sequence_delete_queries);
    kan_dynamic_array_shutdown (&state->indexed_sequence_write_queries);
    kan_dynamic_array_shutdown (&state->indexed_value_read_queries);
    kan_dynamic_array_shutdown (&state->indexed_value_update_queries);
    kan_dynamic_array_shutdown (&state->indexed_value_delete_queries);
    kan_dynamic_array_shutdown (&state->indexed_value_write_queries);
    kan_dynamic_array_shutdown (&state->indexed_signal_read_queries);
    kan_dynamic_array_shutdown (&state->indexed_signal_update_queries);
    kan_dynamic_array_shutdown (&state->indexed_signal_delete_queries);
    kan_dynamic_array_shutdown (&state->indexed_signal_write_queries);
    kan_dynamic_array_shutdown (&state->indexed_interval_read_queries);
    kan_dynamic_array_shutdown (&state->indexed_interval_update_queries);
    kan_dynamic_array_shutdown (&state->indexed_interval_delete_queries);
    kan_dynamic_array_shutdown (&state->indexed_interval_write_queries);
    kan_dynamic_array_shutdown (&state->event_insert_queries);
    kan_dynamic_array_shutdown (&state->event_fetch_queries);
}

static void scan_init (void)
{
    scan.allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "scan");
    scan.scan_bound_state_index = UINT64_MAX;
    kan_dynamic_array_init (&scan.states, 0u, sizeof (struct scanned_state_t), _Alignof (struct scanned_state_t),
                            scan.allocation_group);
}

static void scan_shutdown (void)
{
    for (uint64_t index = 0u; index < scan.states.size; ++index)
    {
        scanned_state_shutdown (&((struct scanned_state_t *) scan.states.data)[index]);
    }

    kan_dynamic_array_shutdown (&scan.states);
}

static inline kan_bool_t scan_generate_state_queries (const char *name_begin, const char *name_end)
{
    const kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    for (uint64_t index = 0u; index < scan.states.size; ++index)
    {
        struct scanned_state_t *state = &((struct scanned_state_t *) scan.states.data)[index];
        if (state->name == name)
        {
            fprintf (stderr,
                     "Error. [%ld:%ld]: Caught attempt to generate state \"%s\" queries in two different places.\n",
                     (long) io.cursor_line, (long) io.cursor_symbol, name);
            return KAN_FALSE;
        }
    }

    struct scanned_state_t *new_state = kan_dynamic_array_add_last (&scan.states);
    if (!new_state)
    {
        kan_dynamic_array_set_capacity (&scan.states, KAN_MAX (1u, scan.states.size * 2u));
        new_state = kan_dynamic_array_add_last (&scan.states);
        KAN_ASSERT (new_state)
    }

    scanned_state_init (new_state);
    new_state->name = name;
    return KAN_TRUE;
}

static inline kan_bool_t scan_bind_state (const char *name_begin,
                                          const char *name_end,
                                          const char *path_begin,
                                          const char *path_end)
{
    const kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    for (uint64_t index = 0u; index < scan.states.size; ++index)
    {
        struct scanned_state_t *state = &((struct scanned_state_t *) scan.states.data)[index];
        if (state->name == name)
        {
            scan.scan_bound_state_index = index;
            return KAN_TRUE;
        }
    }

    fprintf (stderr,
             "Error. [%ld:%ld]: Caught attempt to bind state \"%s\" which is not previously declared through "
             "KAN_UP_GENERATE_STATE_QUERIES.\n",
             (long) io.cursor_line, (long) io.cursor_symbol, name);
    return KAN_FALSE;
}

static inline kan_bool_t scan_ensure_state_bound (void)
{
    if (scan.scan_bound_state_index >= scan.states.size)
    {
        fprintf (stderr,
                 "Error. [%ld:%ld]: Caught attempt to use query without previously binding state through "
                 "KAN_UP_BIND_STATE.\n",
                 (long) io.cursor_line, (long) io.cursor_symbol);
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline void insert_unique_interned_string_into_array (struct kan_dynamic_array_t *array,
                                                             kan_interned_string_t value)
{
    for (uint64_t index = 0u; index < array->size; ++index)
    {
        if (value == ((kan_interned_string_t *) array->data)[index])
        {
            // Already present.
            return;
        }
    }

    void *spot = kan_dynamic_array_add_last (array);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (array, KAN_MAX (1u, array->size * 2u));
        spot = kan_dynamic_array_add_last (array);
        KAN_ASSERT (spot)
    }

    *(kan_interned_string_t *) spot = value;
}

static inline kan_bool_t scan_singleton (const char *name_begin,
                                         const char *name_end,
                                         enum singleton_access_type_t access,
                                         const char *type_begin,
                                         const char *type_end)
{
    if (!scan_ensure_state_bound ())
    {
        return KAN_FALSE;
    }

    struct scanned_state_t *state = &((struct scanned_state_t *) scan.states.data)[scan.scan_bound_state_index];
    struct kan_dynamic_array_t *target_array = NULL;

    switch (access)
    {
    case SINGLETON_ACCESS_TYPE_READ:
        target_array = &state->singleton_read_queries;
        break;

    case SINGLETON_ACCESS_TYPE_WRITE:
        target_array = &state->singleton_write_queries;
        break;
    }

    insert_unique_interned_string_into_array (target_array, kan_char_sequence_intern (type_begin, type_end));
    return KAN_TRUE;
}

static inline kan_bool_t scan_indexed_insert (const char *name_begin,
                                              const char *name_end,
                                              const char *type_begin,
                                              const char *type_end)
{
    if (!scan_ensure_state_bound ())
    {
        return KAN_FALSE;
    }

    struct scanned_state_t *state = &((struct scanned_state_t *) scan.states.data)[scan.scan_bound_state_index];
    insert_unique_interned_string_into_array (&state->indexed_insert_queries,
                                              kan_char_sequence_intern (type_begin, type_end));
    return KAN_TRUE;
}

static inline kan_bool_t scan_sequence (const char *name_begin,
                                        const char *name_end,
                                        enum indexed_access_type_t access,
                                        const char *type_begin,
                                        const char *type_end)
{
    if (!scan_ensure_state_bound ())
    {
        return KAN_FALSE;
    }

    struct scanned_state_t *state = &((struct scanned_state_t *) scan.states.data)[scan.scan_bound_state_index];
    struct kan_dynamic_array_t *target_array = NULL;

    switch (access)
    {
    case INDEXED_ACCESS_TYPE_READ:
        target_array = &state->indexed_sequence_read_queries;
        break;

    case INDEXED_ACCESS_TYPE_UPDATE:
        target_array = &state->indexed_sequence_update_queries;
        break;

    case INDEXED_ACCESS_TYPE_DELETE:
        target_array = &state->indexed_sequence_delete_queries;
        break;

    case INDEXED_ACCESS_TYPE_WRITE:
        target_array = &state->indexed_sequence_write_queries;
        break;
    }

    insert_unique_interned_string_into_array (target_array, kan_char_sequence_intern (type_begin, type_end));
    return KAN_TRUE;
}

static inline kan_bool_t insert_unique_scanned_indexed_field_query_into_array (struct kan_dynamic_array_t *array,
                                                                               kan_interned_string_t type,
                                                                               const char *field_begin,
                                                                               const char *field_end)
{
    const uint64_t field_length = field_end - field_begin;
    if (field_length > KAN_UNIVERSE_PREPROCESSOR_TARGET_PATH_MAX_LENGTH - 1u)
    {
        fprintf (stderr, "Error. [%ld:%ld]: Found field path \"%s\" that is longer than maximum %d.\n",
                 (long) io.cursor_line, (long) io.cursor_symbol, kan_char_sequence_intern (field_begin, field_end),
                 (int) (KAN_UNIVERSE_PREPROCESSOR_TARGET_PATH_MAX_LENGTH - 1u));
        return KAN_FALSE;
    }

    for (uint64_t index = 0u; index < array->size; ++index)
    {
        struct scanned_indexed_field_query_t *query = &((struct scanned_indexed_field_query_t *) array->data)[index];
        if (query->type == type && strncmp (field_begin, query->field_path, field_length) == 0)
        {
            // Already present.
            return KAN_TRUE;
        }
    }

    struct scanned_indexed_field_query_t *spot = kan_dynamic_array_add_last (array);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (array, KAN_MAX (1u, array->size * 2u));
        spot = kan_dynamic_array_add_last (array);
        KAN_ASSERT (spot)
    }

    spot->type = type;
    memcpy (spot->field_path, field_begin, field_length);
    spot->field_path[field_length] = '\0';
    return KAN_TRUE;
}

static inline kan_bool_t scan_value (const char *name_begin,
                                     const char *name_end,
                                     enum indexed_access_type_t access,
                                     const char *type_begin,
                                     const char *type_end,
                                     const char *field_begin,
                                     const char *field_end,
                                     const char *argument_begin,
                                     const char *argument_end)
{
    if (!scan_ensure_state_bound ())
    {
        return KAN_FALSE;
    }

    struct scanned_state_t *state = &((struct scanned_state_t *) scan.states.data)[scan.scan_bound_state_index];
    struct kan_dynamic_array_t *target_array = NULL;

    switch (access)
    {
    case INDEXED_ACCESS_TYPE_READ:
        target_array = &state->indexed_value_read_queries;
        break;

    case INDEXED_ACCESS_TYPE_UPDATE:
        target_array = &state->indexed_value_update_queries;
        break;

    case INDEXED_ACCESS_TYPE_DELETE:
        target_array = &state->indexed_value_delete_queries;
        break;

    case INDEXED_ACCESS_TYPE_WRITE:
        target_array = &state->indexed_value_write_queries;
        break;
    }

    insert_unique_scanned_indexed_field_query_into_array (target_array, kan_char_sequence_intern (type_begin, type_end),
                                                          field_begin, field_end);
    return KAN_TRUE;
}

static inline kan_bool_t insert_unique_scanned_signal_query_into_array (struct kan_dynamic_array_t *array,
                                                                        kan_interned_string_t type,
                                                                        const char *field_begin,
                                                                        const char *field_end,
                                                                        const char *value_begin,
                                                                        const char *value_end)
{
    const uint64_t field_length = field_end - field_begin;
    if (field_length > KAN_UNIVERSE_PREPROCESSOR_TARGET_PATH_MAX_LENGTH - 1u)
    {
        fprintf (stderr, "Error. [%ld:%ld]: Found field path \"%s\" that is longer than maximum %d.\n",
                 (long) io.cursor_line, (long) io.cursor_symbol, kan_char_sequence_intern (field_begin, field_end),
                 (int) (KAN_UNIVERSE_PREPROCESSOR_TARGET_PATH_MAX_LENGTH - 1u));
        return KAN_FALSE;
    }

    const uint64_t value_length = value_end - value_begin;
    if (value_length > KAN_UNIVERSE_PREPROCESSOR_SIGNAL_VALUE_MAX_LENGTH - 1u)
    {
        fprintf (stderr, "Error. [%ld:%ld]: Found signal value literal \"%s\" that is longer than maximum %d.\n",
                 (long) io.cursor_line, (long) io.cursor_symbol, kan_char_sequence_intern (value_begin, value_end),
                 (int) (KAN_UNIVERSE_PREPROCESSOR_TARGET_PATH_MAX_LENGTH - 1u));
        return KAN_FALSE;
    }

    for (uint64_t index = 0u; index < array->size; ++index)
    {
        struct scanned_signal_query_t *query = &((struct scanned_signal_query_t *) array->data)[index];
        if (query->type == type && strncmp (field_begin, query->field_path, field_length) == 0 &&
            strncmp (value_begin, query->signal_value, value_length) == 0)
        {
            // Already present.
            return KAN_TRUE;
        }
    }

    struct scanned_signal_query_t *spot = kan_dynamic_array_add_last (array);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (array, KAN_MAX (1u, array->size * 2u));
        spot = kan_dynamic_array_add_last (array);
        KAN_ASSERT (spot)
    }

    spot->type = type;
    memcpy (spot->field_path, field_begin, field_length);
    spot->field_path[field_length] = '\0';
    memcpy (spot->signal_value, value_begin, value_length);
    spot->signal_value[value_length] = '\0';
    return KAN_TRUE;
}

static inline kan_bool_t scan_signal (const char *name_begin,
                                      const char *name_end,
                                      enum indexed_access_type_t access,
                                      const char *type_begin,
                                      const char *type_end,
                                      const char *field_begin,
                                      const char *field_end,
                                      const char *value_begin,
                                      const char *value_end)
{
    if (!scan_ensure_state_bound ())
    {
        return KAN_FALSE;
    }

    struct scanned_state_t *state = &((struct scanned_state_t *) scan.states.data)[scan.scan_bound_state_index];
    struct kan_dynamic_array_t *target_array = NULL;

    switch (access)
    {
    case INDEXED_ACCESS_TYPE_READ:
        target_array = &state->indexed_signal_read_queries;
        break;

    case INDEXED_ACCESS_TYPE_UPDATE:
        target_array = &state->indexed_signal_update_queries;
        break;

    case INDEXED_ACCESS_TYPE_DELETE:
        target_array = &state->indexed_signal_delete_queries;
        break;

    case INDEXED_ACCESS_TYPE_WRITE:
        target_array = &state->indexed_signal_write_queries;
        break;
    }

    insert_unique_scanned_signal_query_into_array (target_array, kan_char_sequence_intern (type_begin, type_end),
                                                   field_begin, field_end, value_begin, value_end);
    return KAN_TRUE;
}

static inline kan_bool_t scan_interval (const char *name_begin,
                                        const char *name_end,
                                        enum indexed_access_type_t access,
                                        const char *type_begin,
                                        const char *type_end,
                                        const char *field_begin,
                                        const char *field_end,
                                        const char *argument_min_begin,
                                        const char *argument_min_end,
                                        const char *argument_max_begin,
                                        const char *argument_max_end)
{
    if (!scan_ensure_state_bound ())
    {
        return KAN_FALSE;
    }

    struct scanned_state_t *state = &((struct scanned_state_t *) scan.states.data)[scan.scan_bound_state_index];
    struct kan_dynamic_array_t *target_array = NULL;

    switch (access)
    {
    case INDEXED_ACCESS_TYPE_READ:
        target_array = &state->indexed_interval_read_queries;
        break;

    case INDEXED_ACCESS_TYPE_UPDATE:
        target_array = &state->indexed_interval_update_queries;
        break;

    case INDEXED_ACCESS_TYPE_DELETE:
        target_array = &state->indexed_interval_delete_queries;
        break;

    case INDEXED_ACCESS_TYPE_WRITE:
        target_array = &state->indexed_interval_write_queries;
        break;
    }

    insert_unique_scanned_indexed_field_query_into_array (target_array, kan_char_sequence_intern (type_begin, type_end),
                                                          field_begin, field_end);
    return KAN_TRUE;
}

static kan_bool_t scan_event_insert (const char *name_begin,
                                     const char *name_end,
                                     const char *type_begin,
                                     const char *type_end)
{
    if (!scan_ensure_state_bound ())
    {
        return KAN_FALSE;
    }

    struct scanned_state_t *state = &((struct scanned_state_t *) scan.states.data)[scan.scan_bound_state_index];
    insert_unique_interned_string_into_array (&state->event_insert_queries,
                                              kan_char_sequence_intern (type_begin, type_end));
    return KAN_TRUE;
}

static kan_bool_t scan_event_fetch (const char *name_begin,
                                    const char *name_end,
                                    const char *type_begin,
                                    const char *type_end)
{
    if (!scan_ensure_state_bound ())
    {
        return KAN_FALSE;
    }

    struct scanned_state_t *state = &((struct scanned_state_t *) scan.states.data)[scan.scan_bound_state_index];
    insert_unique_interned_string_into_array (&state->event_fetch_queries,
                                              kan_char_sequence_intern (type_begin, type_end));
    return KAN_TRUE;
}

// Process phase functions.

static inline kan_bool_t output_string (const char *string)
{
    const uint64_t string_length = strlen (string);
    if (string_length == 0u)
    {
        return KAN_TRUE;
    }

    for (uint64_t index = 0u; index < string_length; ++index)
    {
        if (string[index] == '\n')
        {
            ++process.output_file_line;
        }
    }

    return io.output_stream->operations->write (io.output_stream, string_length, string) == string_length;
}

static inline kan_bool_t output_sequence (const char *begin, const char *end)
{
    const uint64_t string_length = end - begin;
    if (string_length == 0u)
    {
        return KAN_TRUE;
    }

    for (uint64_t index = 0u; index < string_length; ++index)
    {
        if (begin[index] == '\n')
        {
            ++process.output_file_line;
        }
    }

    return io.output_stream->operations->write (io.output_stream, string_length, begin) == string_length;
}

static inline kan_bool_t output_use_source_line (void)
{
    char number_buffer[32u];
    snprintf (number_buffer, 32u, "%lu", (unsigned long) io.cursor_line);
    return output_string ("#line ") && output_string (number_buffer) && output_string (" \"") &&
           output_string (arguments.input_file_path) && output_string ("\"\n");
}

static inline kan_bool_t output_use_output_line (void)
{
    char number_buffer[32u];
    snprintf (number_buffer, 32u, "%lu", (unsigned long) process.output_file_line + 1u);
    return output_string ("#line ") && output_string (number_buffer) && output_string (" \"") &&
           output_string (arguments.output_file_path) && output_string ("\"\n");
}

static inline kan_bool_t output_markup_macro_comment (void)
{
    KAN_ASSERT (io.cursor - 1u > io.token)
    if (*(io.cursor - 1u) == '\n')
    {
        return output_string ("/* ") && output_sequence (io.token, io.cursor - 1u) && output_string (" */\n");
    }
    else
    {
        return output_string ("/* ") && output_sequence (io.token, io.cursor) && output_string (" */");
    }
}

static inline kan_bool_t output_field_path (const char *field_path)
{
    const char *part_begin = field_path;
    while (*field_path)
    {
        if (*field_path == '.')
        {
            if (!output_sequence (part_begin, field_path) || !output_string ("__"))
            {
                return KAN_FALSE;
            }

            ++field_path;
            part_begin = field_path;
        }
        else
        {
            ++field_path;
        }
    }

    return part_begin == field_path || output_sequence (part_begin, field_path);
}

static inline kan_bool_t output_field_path_sequence (const char *field_path_begin, const char *field_path_end)
{
    const char *part_begin = field_path_begin;
    while (field_path_begin != field_path_end)
    {
        if (*field_path_begin == '.')
        {
            if (!output_sequence (part_begin, field_path_begin) || !output_string ("__"))
            {
                return KAN_FALSE;
            }

            ++field_path_begin;
            part_begin = field_path_begin;
        }
        else
        {
            ++field_path_begin;
        }
    }

    return part_begin == field_path_begin || output_sequence (part_begin, field_path_begin);
}

static inline enum parse_response_t process_generate_state_queries (const char *name_begin, const char *name_end)
{
    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    struct scanned_state_t *state = NULL;

    for (uint64_t index = 0u; index < scan.states.size; ++index)
    {
        struct scanned_state_t *other_state = &((struct scanned_state_t *) scan.states.data)[index];
        if (other_state->name == name)
        {
            state = other_state;
            break;
        }
    }

    if (!state)
    {
        // No state? Should've been handled by scan phase.
        KAN_ASSERT (KAN_FALSE)
        return PARSE_RESPONSE_FAILED;
    }

    if (!output_markup_macro_comment () || !output_use_output_line ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    for (uint64_t index = 0u; index < state->singleton_read_queries.size; ++index)
    {
        if (!output_string ("struct kan_repository_singleton_read_query_t read__") ||
            !output_string (((kan_interned_string_t *) state->singleton_read_queries.data)[index]) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->singleton_write_queries.size; ++index)
    {
        if (!output_string ("struct kan_repository_singleton_write_query_t write__") ||
            !output_string (((kan_interned_string_t *) state->singleton_write_queries.data)[index]) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_insert_queries.size; ++index)
    {
        if (!output_string ("struct kan_repository_indexed_insert_query_t insert__") ||
            !output_string (((kan_interned_string_t *) state->indexed_insert_queries.data)[index]) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_sequence_read_queries.size; ++index)
    {
        if (!output_string ("struct kan_repository_indexed_sequence_read_query_t read_sequence__") ||
            !output_string (((kan_interned_string_t *) state->indexed_sequence_read_queries.data)[index]) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_sequence_update_queries.size; ++index)
    {
        if (!output_string ("struct kan_repository_indexed_sequence_update_query_t update_sequence__") ||
            !output_string (((kan_interned_string_t *) state->indexed_sequence_update_queries.data)[index]) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_sequence_delete_queries.size; ++index)
    {
        if (!output_string ("struct kan_repository_indexed_sequence_delete_query_t delete_sequence__") ||
            !output_string (((kan_interned_string_t *) state->indexed_sequence_delete_queries.data)[index]) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_sequence_write_queries.size; ++index)
    {
        if (!output_string ("struct kan_repository_indexed_sequence_write_query_t write_sequence__") ||
            !output_string (((kan_interned_string_t *) state->indexed_sequence_write_queries.data)[index]) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_value_read_queries.size; ++index)
    {
        struct scanned_indexed_field_query_t *query =
            &((struct scanned_indexed_field_query_t *) state->indexed_value_read_queries.data)[index];

        if (!output_string ("struct kan_repository_indexed_value_read_query_t read_value__") ||
            !output_string (query->type) || !output_string ("__") || !output_field_path (query->field_path) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_value_update_queries.size; ++index)
    {
        struct scanned_indexed_field_query_t *query =
            &((struct scanned_indexed_field_query_t *) state->indexed_value_update_queries.data)[index];

        if (!output_string ("struct kan_repository_indexed_value_update_query_t update_value__") ||
            !output_string (query->type) || !output_string ("__") || !output_field_path (query->field_path) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_value_delete_queries.size; ++index)
    {
        struct scanned_indexed_field_query_t *query =
            &((struct scanned_indexed_field_query_t *) state->indexed_value_delete_queries.data)[index];

        if (!output_string ("struct kan_repository_indexed_value_delete_query_t delete_value__") ||
            !output_string (query->type) || !output_string ("__") || !output_field_path (query->field_path) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_value_write_queries.size; ++index)
    {
        struct scanned_indexed_field_query_t *query =
            &((struct scanned_indexed_field_query_t *) state->indexed_value_write_queries.data)[index];

        if (!output_string ("struct kan_repository_indexed_value_write_query_t write_value__") ||
            !output_string (query->type) || !output_string ("__") || !output_field_path (query->field_path) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_signal_read_queries.size; ++index)
    {
        struct scanned_signal_query_t *query =
            &((struct scanned_signal_query_t *) state->indexed_signal_read_queries.data)[index];

        if (!output_string ("struct kan_repository_indexed_signal_read_query_t read_value__") ||
            !output_string (query->type) || !output_string ("__") || !output_field_path (query->field_path) ||
            !output_string ("__") || !output_string (query->signal_value) || !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_signal_update_queries.size; ++index)
    {
        struct scanned_signal_query_t *query =
            &((struct scanned_signal_query_t *) state->indexed_signal_update_queries.data)[index];

        if (!output_string ("struct kan_repository_indexed_signal_update_query_t update_value__") ||
            !output_string (query->type) || !output_string ("__") || !output_field_path (query->field_path) ||
            !output_string ("__") || !output_string (query->signal_value) || !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_signal_delete_queries.size; ++index)
    {
        struct scanned_signal_query_t *query =
            &((struct scanned_signal_query_t *) state->indexed_signal_delete_queries.data)[index];

        if (!output_string ("struct kan_repository_indexed_signal_delete_query_t delete_value__") ||
            !output_string (query->type) || !output_string ("__") || !output_field_path (query->field_path) ||
            !output_string ("__") || !output_string (query->signal_value) || !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_signal_write_queries.size; ++index)
    {
        struct scanned_signal_query_t *query =
            &((struct scanned_signal_query_t *) state->indexed_signal_write_queries.data)[index];

        if (!output_string ("struct kan_repository_indexed_signal_write_query_t write_value__") ||
            !output_string (query->type) || !output_string ("__") || !output_field_path (query->field_path) ||
            !output_string ("__") || !output_string (query->signal_value) || !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_interval_read_queries.size; ++index)
    {
        struct scanned_indexed_field_query_t *query =
            &((struct scanned_indexed_field_query_t *) state->indexed_interval_read_queries.data)[index];

        if (!output_string ("struct kan_repository_indexed_interval_read_query_t read_interval__") ||
            !output_string (query->type) || !output_string ("__") || !output_field_path (query->field_path) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_interval_update_queries.size; ++index)
    {
        struct scanned_indexed_field_query_t *query =
            &((struct scanned_indexed_field_query_t *) state->indexed_interval_update_queries.data)[index];

        if (!output_string ("struct kan_repository_indexed_interval_update_query_t update_interval__") ||
            !output_string (query->type) || !output_string ("__") || !output_field_path (query->field_path) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_interval_delete_queries.size; ++index)
    {
        struct scanned_indexed_field_query_t *query =
            &((struct scanned_indexed_field_query_t *) state->indexed_interval_delete_queries.data)[index];

        if (!output_string ("struct kan_repository_indexed_interval_delete_query_t delete_interval__") ||
            !output_string (query->type) || !output_string ("__") || !output_field_path (query->field_path) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->indexed_interval_write_queries.size; ++index)
    {
        struct scanned_indexed_field_query_t *query =
            &((struct scanned_indexed_field_query_t *) state->indexed_interval_write_queries.data)[index];

        if (!output_string ("struct kan_repository_indexed_interval_write_query_t write_interval__") ||
            !output_string (query->type) || !output_string ("__") || !output_field_path (query->field_path) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->event_insert_queries.size; ++index)
    {
        if (!output_string ("struct kan_repository_event_insert_query_t insert__") ||
            !output_string (((kan_interned_string_t *) state->event_insert_queries.data)[index]) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    for (uint64_t index = 0u; index < state->event_fetch_queries.size; ++index)
    {
        if (!output_string ("struct kan_repository_event_fetch_query_t fetch__") ||
            !output_string (((kan_interned_string_t *) state->event_fetch_queries.data)[index]) ||
            !output_string (";\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    if (!output_use_source_line ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_bind_state (const char *name_begin,
                                                        const char *name_end,
                                                        const char *path_begin,
                                                        const char *path_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    process.bound_state = NULL;

    for (uint64_t index = 0u; index < scan.states.size; ++index)
    {
        struct scanned_state_t *other_state = &((struct scanned_state_t *) scan.states.data)[index];
        if (other_state->name == name)
        {
            process.bound_state = other_state;
            break;
        }
    }

    if (!process.bound_state)
    {
        // No state? Should've been handled by scan phase.
        KAN_ASSERT (KAN_FALSE)
        return PARSE_RESPONSE_FAILED;
    }

    memcpy (process.bound_state_path, path_begin, path_end - path_begin);
    process.bound_state_path[path_end - path_begin] = '\0';
    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline void push_query_stack_node (kan_interned_string_t name, enum process_query_type_t query_type)
{
    struct process_query_stack_node_t *node =
        kan_allocate_batched (process.allocation_group, sizeof (struct process_query_stack_node_t));
    node->previous = process.stack_top;
    process.stack_top = node;
    node->blocks = process.blocks;
    node->name = name;
    node->query_type = query_type;
}

static inline kan_bool_t output_singleton_begin (
    kan_interned_string_t name, const char *type_begin, const char *type_end, const char *if_const, const char *access)
{
    // Should be ensured by previous pass.
    KAN_ASSERT (process.bound_state)

    return output_use_output_line () && output_string ("kan_repository_singleton_") && output_string (access) &&
           output_string ("_access_t ") && output_string (name) &&
           output_string ("_access = kan_repository_singleton_") && output_string (access) &&
           output_string ("_query_execute (&(") && output_string (process.bound_state_path) && output_string (")->") &&
           output_string (access) && output_string ("__") && output_sequence (type_begin, type_end) &&
           output_string (");\n") && output_string (if_const) && output_string ("struct ") &&
           output_sequence (type_begin, type_end) && output_string (" *") && output_string (name) &&
           output_string (" = kan_repository_singleton_") && output_string (access) &&
           output_string ("_access_resolve (") && output_string (name) && output_string ("_access);\nkan_bool_t ") &&
           output_string (name) && output_string ("_access_expired = KAN_FALSE;\n") && output_use_source_line ();
}

static inline enum parse_response_t process_singleton_read (const char *name_begin,
                                                            const char *name_end,
                                                            const char *type_begin,
                                                            const char *type_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_SINGLETON_READ);

    if (!output_singleton_begin (name, type_begin, type_end, "const ", "read"))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_singleton_write (const char *name_begin,
                                                             const char *name_end,
                                                             const char *type_begin,
                                                             const char *type_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_SINGLETON_WRITE);

    if (!output_singleton_begin (name, type_begin, type_end, "", "write"))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_indexed_insert (const char *name_begin,
                                                            const char *name_end,
                                                            const char *type_begin,
                                                            const char *type_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_INDEXED_INSERT);

    kan_bool_t output =
        output_use_output_line () && output_string ("struct kan_repository_indexed_insertion_package_t ") &&
        output_string (name) && output_string ("_package = kan_repository_indexed_insert_query_execute (&(") &&
        output_string (process.bound_state_path) && output_string (")->") && output_string ("insert__") &&
        output_sequence (type_begin, type_end) && output_string (");\n") && output_string ("struct ") &&
        output_sequence (type_begin, type_end) && output_string (" *") && output_string (name) &&
        output_string (" = kan_repository_indexed_insertion_package_get (&") && output_string (name) &&
        output_string ("_package);\n") && output_use_source_line ();

    if (!output)
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline kan_bool_t output_sequence_begin (
    kan_interned_string_t name, const char *type_begin, const char *type_end, const char *if_const, const char *access)
{
    // Should be ensured by previous pass.
    KAN_ASSERT (process.bound_state)

    return output_use_output_line () && output_string ("struct kan_repository_indexed_sequence_") &&
           output_string (access) && output_string ("_cursor_t ") && output_string (name) &&
           output_string ("_cursor = kan_repository_indexed_sequence_") && output_string (access) &&
           output_string ("_query_execute (&(") && output_string (process.bound_state_path) && output_string (")->") &&
           output_string (access) && output_string ("_sequence__") && output_sequence (type_begin, type_end) &&
           output_string (");\nwhile (KAN_TRUE)\n{\n    struct kan_repository_indexed_sequence_") &&
           output_string (access) && output_string ("_access_t ") && output_string (name) &&
           output_string ("_access = kan_repository_indexed_sequence_") && output_string (access) &&
           output_string ("_cursor_next (&") && output_string (name) && output_string ("_cursor);\n    ") &&
           output_string (if_const) && output_string ("struct ") && output_sequence (type_begin, type_end) &&
           output_string (" *") && output_string (name) && output_string (" = kan_repository_indexed_sequence_") &&
           output_string (access) && output_string ("_access_resolve (&") && output_string (name) &&
           output_string ("_access);\n    kan_bool_t ") && output_string (name) &&
           output_string ("_access_expired = KAN_FALSE;\n    if (") && output_string (name) && output_string (")\n") &&
           output_use_source_line ();
}

static inline enum parse_response_t process_sequence_read (const char *name_begin,
                                                           const char *name_end,
                                                           const char *type_begin,
                                                           const char *type_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_SEQUENCE_READ);

    if (!output_sequence_begin (name, type_begin, type_end, "const ", "read"))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_sequence_update (const char *name_begin,
                                                             const char *name_end,
                                                             const char *type_begin,
                                                             const char *type_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_SEQUENCE_UPDATE);

    if (!output_sequence_begin (name, type_begin, type_end, "", "update"))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_sequence_delete (const char *name_begin,
                                                             const char *name_end,
                                                             const char *type_begin,
                                                             const char *type_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_SEQUENCE_DELETE);

    if (!output_sequence_begin (name, type_begin, type_end, "const ", "delete"))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_sequence_write (const char *name_begin,
                                                            const char *name_end,
                                                            const char *type_begin,
                                                            const char *type_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_SEQUENCE_WRITE);

    if (!output_sequence_begin (name, type_begin, type_end, "", "write"))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline kan_bool_t output_value_begin (kan_interned_string_t name,
                                             const char *type_begin,
                                             const char *type_end,
                                             const char *if_const,
                                             const char *access,
                                             const char *field_begin,
                                             const char *field_end,
                                             const char *argument_begin,
                                             const char *argument_end)
{
    // Should be ensured by previous pass.
    KAN_ASSERT (process.bound_state)

    return output_use_output_line () && output_string ("struct kan_repository_indexed_value_") &&
           output_string (access) && output_string ("_cursor_t ") && output_string (name) &&
           output_string ("_cursor = kan_repository_indexed_value_") && output_string (access) &&
           output_string ("_query_execute (&(") && output_string (process.bound_state_path) && output_string (")->") &&
           output_string (access) && output_string ("_value__") && output_sequence (type_begin, type_end) &&
           output_string ("__") && output_field_path_sequence (field_begin, field_end) && output_string (", ") &&
           output_sequence (argument_begin, argument_end) &&
           output_string (");\nwhile (KAN_TRUE)\n{\n    struct kan_repository_indexed_value_") &&
           output_string (access) && output_string ("_access_t ") && output_string (name) &&
           output_string ("_access = kan_repository_indexed_value_") && output_string (access) &&
           output_string ("_cursor_next (&") && output_string (name) && output_string ("_cursor);\n    ") &&
           output_string (if_const) && output_string ("struct ") && output_sequence (type_begin, type_end) &&
           output_string (" *") && output_string (name) && output_string (" = kan_repository_indexed_value_") &&
           output_string (access) && output_string ("_access_resolve (&") && output_string (name) &&
           output_string ("_access);\n    kan_bool_t ") && output_string (name) &&
           output_string ("_access_expired = KAN_FALSE;\n    if (") && output_string (name) && output_string (")\n") &&
           output_use_source_line ();
}

static inline enum parse_response_t process_value_read (const char *name_begin,
                                                        const char *name_end,
                                                        const char *type_begin,
                                                        const char *type_end,
                                                        const char *field_begin,
                                                        const char *field_end,
                                                        const char *argument_begin,
                                                        const char *argument_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_VALUE_READ);

    if (!output_value_begin (name, type_begin, type_end, "const ", "read", field_begin, field_end, argument_begin,
                             argument_end))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_value_update (const char *name_begin,
                                                          const char *name_end,
                                                          const char *type_begin,
                                                          const char *type_end,
                                                          const char *field_begin,
                                                          const char *field_end,
                                                          const char *argument_begin,
                                                          const char *argument_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_VALUE_UPDATE);

    if (!output_value_begin (name, type_begin, type_end, "", "update", field_begin, field_end, argument_begin,
                             argument_end))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_value_delete (const char *name_begin,
                                                          const char *name_end,
                                                          const char *type_begin,
                                                          const char *type_end,
                                                          const char *field_begin,
                                                          const char *field_end,
                                                          const char *argument_begin,
                                                          const char *argument_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_VALUE_DELETE);

    if (!output_value_begin (name, type_begin, type_end, "const ", "delete", field_begin, field_end, argument_begin,
                             argument_end))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_value_write (const char *name_begin,
                                                         const char *name_end,
                                                         const char *type_begin,
                                                         const char *type_end,
                                                         const char *field_begin,
                                                         const char *field_end,
                                                         const char *argument_begin,
                                                         const char *argument_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_VALUE_WRITE);

    if (!output_value_begin (name, type_begin, type_end, "", "write", field_begin, field_end, argument_begin,
                             argument_end))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline kan_bool_t output_signal_begin (kan_interned_string_t name,
                                              const char *type_begin,
                                              const char *type_end,
                                              const char *if_const,
                                              const char *access,
                                              const char *field_begin,
                                              const char *field_end,
                                              const char *signal_value_begin,
                                              const char *signal_value_end)
{
    // Should be ensured by previous pass.
    KAN_ASSERT (process.bound_state)

    return output_use_output_line () && output_string ("struct kan_repository_indexed_signal_") &&
           output_string (access) && output_string ("_cursor_t ") && output_string (name) &&
           output_string ("_cursor = kan_repository_indexed_signal_") && output_string (access) &&
           output_string ("_query_execute (&(") && output_string (process.bound_state_path) && output_string (")->") &&
           output_string (access) && output_string ("_signal__") && output_sequence (type_begin, type_end) &&
           output_string ("__") && output_field_path_sequence (field_begin, field_end) && output_string ("__") &&
           output_sequence (signal_value_begin, signal_value_end) &&
           output_string (");\nwhile (KAN_TRUE)\n{\n    struct kan_repository_indexed_signal_") &&
           output_string (access) && output_string ("_access_t ") && output_string (name) &&
           output_string ("_access = kan_repository_indexed_signal_") && output_string (access) &&
           output_string ("_cursor_next (&") && output_string (name) && output_string ("_cursor);\n    ") &&
           output_string (if_const) && output_string ("struct ") && output_sequence (type_begin, type_end) &&
           output_string (" *") && output_string (name) && output_string (" = kan_repository_indexed_signal_") &&
           output_string (access) && output_string ("_access_resolve (&") && output_string (name) &&
           output_string ("_access);\n    kan_bool_t ") && output_string (name) &&
           output_string ("_access_expired = KAN_FALSE;\n    if (") && output_string (name) && output_string (")\n") &&
           output_use_source_line ();
}

static inline enum parse_response_t process_signal_read (const char *name_begin,
                                                         const char *name_end,
                                                         const char *type_begin,
                                                         const char *type_end,
                                                         const char *field_begin,
                                                         const char *field_end,
                                                         const char *signal_value_begin,
                                                         const char *signal_value_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_SIGNAL_READ);

    if (!output_signal_begin (name, type_begin, type_end, "const ", "read", field_begin, field_end, signal_value_begin,
                              signal_value_end))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_signal_update (const char *name_begin,
                                                           const char *name_end,
                                                           const char *type_begin,
                                                           const char *type_end,
                                                           const char *field_begin,
                                                           const char *field_end,
                                                           const char *signal_value_begin,
                                                           const char *signal_value_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_SIGNAL_UPDATE);

    if (!output_signal_begin (name, type_begin, type_end, "", "update", field_begin, field_end, signal_value_begin,
                              signal_value_end))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_signal_delete (const char *name_begin,
                                                           const char *name_end,
                                                           const char *type_begin,
                                                           const char *type_end,
                                                           const char *field_begin,
                                                           const char *field_end,
                                                           const char *signal_value_begin,
                                                           const char *signal_value_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_SIGNAL_DELETE);

    if (!output_signal_begin (name, type_begin, type_end, "const ", "delete", field_begin, field_end,
                              signal_value_begin, signal_value_end))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_signal_write (const char *name_begin,
                                                          const char *name_end,
                                                          const char *type_begin,
                                                          const char *type_end,
                                                          const char *field_begin,
                                                          const char *field_end,
                                                          const char *signal_value_begin,
                                                          const char *signal_value_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_SIGNAL_WRITE);

    if (!output_signal_begin (name, type_begin, type_end, "", "write", field_begin, field_end, signal_value_begin,
                              signal_value_end))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline kan_bool_t output_interval_begin (kan_interned_string_t name,
                                                const char *type_begin,
                                                const char *type_end,
                                                const char *if_const,
                                                const char *access,
                                                const char *direction,
                                                const char *field_begin,
                                                const char *field_end,
                                                const char *argument_min_begin,
                                                const char *argument_min_end,
                                                const char *argument_max_begin,
                                                const char *argument_max_end)
{
    // Should be ensured by previous pass.
    KAN_ASSERT (process.bound_state)

    return output_use_output_line () && output_string ("struct kan_repository_indexed_interval_") &&
           output_string (direction) && output_string ("_") && output_string (access) && output_string ("_cursor_t ") &&
           output_string (name) && output_string ("_cursor = kan_repository_indexed_interval_") &&
           output_string (access) && output_string ("_query_execute_") && output_string (direction) &&
           output_string (" (&(") && output_string (process.bound_state_path) && output_string (")->") &&
           output_string (access) && output_string ("_interval__") && output_sequence (type_begin, type_end) &&
           output_string ("__") && output_field_path_sequence (field_begin, field_end) && output_string (", ") &&
           output_sequence (argument_min_begin, argument_min_end) && output_string (", ") &&
           output_sequence (argument_max_begin, argument_max_end) &&
           output_string (");\nwhile (KAN_TRUE)\n{\n    struct kan_repository_indexed_interval_") &&
           output_string (access) && output_string ("_access_t ") && output_string (name) &&
           output_string ("_access = kan_repository_indexed_interval_") && output_string (direction) &&
           output_string ("_") && output_string (access) && output_string ("_cursor_next (&") && output_string (name) &&
           output_string ("_cursor);\n    ") && output_string (if_const) && output_string ("struct ") &&
           output_sequence (type_begin, type_end) && output_string (" *") && output_string (name) &&
           output_string (" = kan_repository_indexed_interval_") && output_string (access) &&
           output_string ("_access_resolve (&") && output_string (name) &&
           output_string ("_access);\n    kan_bool_t ") && output_string (name) &&
           output_string ("_access_expired = KAN_FALSE;\n    if (") && output_string (name) && output_string (")\n") &&
           output_use_source_line ();
}

static inline enum parse_response_t process_interval_read (const char *name_begin,
                                                           const char *name_end,
                                                           const char *type_begin,
                                                           const char *type_end,
                                                           const char *field_begin,
                                                           const char *field_end,
                                                           kan_bool_t ascending,
                                                           const char *argument_min_begin,
                                                           const char *argument_min_end,
                                                           const char *argument_max_begin,
                                                           const char *argument_max_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (
        name, ascending ? PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_READ : PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_READ);

    if (!output_interval_begin (name, type_begin, type_end, "const ", "read", ascending ? "ascending" : "descending",
                                field_begin, field_end, argument_min_begin, argument_min_end, argument_max_begin,
                                argument_max_end))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_interval_update (const char *name_begin,
                                                             const char *name_end,
                                                             const char *type_begin,
                                                             const char *type_end,
                                                             const char *field_begin,
                                                             const char *field_end,
                                                             kan_bool_t ascending,
                                                             const char *argument_min_begin,
                                                             const char *argument_min_end,
                                                             const char *argument_max_begin,
                                                             const char *argument_max_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (
        name, ascending ? PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_UPDATE : PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_UPDATE);

    if (!output_interval_begin (name, type_begin, type_end, "", "update", ascending ? "ascending" : "descending",
                                field_begin, field_end, argument_min_begin, argument_min_end, argument_max_begin,
                                argument_max_end))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_interval_delete (const char *name_begin,
                                                             const char *name_end,
                                                             const char *type_begin,
                                                             const char *type_end,
                                                             const char *field_begin,
                                                             const char *field_end,
                                                             kan_bool_t ascending,
                                                             const char *argument_min_begin,
                                                             const char *argument_min_end,
                                                             const char *argument_max_begin,
                                                             const char *argument_max_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (
        name, ascending ? PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_DELETE : PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_DELETE);

    if (!output_interval_begin (name, type_begin, type_end, "const ", "delete", ascending ? "ascending" : "descending",
                                field_begin, field_end, argument_min_begin, argument_min_end, argument_max_begin,
                                argument_max_end))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_interval_write (const char *name_begin,
                                                            const char *name_end,
                                                            const char *type_begin,
                                                            const char *type_end,
                                                            const char *field_begin,
                                                            const char *field_end,
                                                            kan_bool_t ascending,
                                                            const char *argument_min_begin,
                                                            const char *argument_min_end,
                                                            const char *argument_max_begin,
                                                            const char *argument_max_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (
        name, ascending ? PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_WRITE : PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_WRITE);

    if (!output_interval_begin (name, type_begin, type_end, "", "write", ascending ? "ascending" : "descending",
                                field_begin, field_end, argument_min_begin, argument_min_end, argument_max_begin,
                                argument_max_end))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_event_insert (const char *name_begin,
                                                          const char *name_end,
                                                          const char *type_begin,
                                                          const char *type_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_EVENT_INSERT);

    kan_bool_t output =
        output_use_output_line () && output_string ("struct kan_repository_event_insertion_package_t ") &&
        output_string (name) && output_string ("_package = kan_repository_event_insert_query_execute (&(") &&
        output_string (process.bound_state_path) && output_string (")->") && output_string ("insert__") &&
        output_sequence (type_begin, type_end) && output_string (");\n") && output_string ("struct ") &&
        output_sequence (type_begin, type_end) && output_string (" *") && output_string (name) &&
        output_string (" = kan_repository_event_insertion_package_get (&") && output_string (name) &&
        output_string ("_package);\nif (") && output_string (name) && output_string (")\n") &&
        output_use_source_line ();

    if (!output)
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_event_fetch (const char *name_begin,
                                                         const char *name_end,
                                                         const char *type_begin,
                                                         const char *type_end)
{
    if (!output_markup_macro_comment ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    push_query_stack_node (name, PROCESS_QUERY_TYPE_EVENT_FETCH);

    kan_bool_t output = output_use_output_line () &&
                        output_string ("while (KAN_TRUE)\n{\n    struct kan_repository_event_read_access_t ") &&
                        output_string (name) && output_string ("_access = kan_repository_event_fetch_query_next (&(") &&
                        output_string (process.bound_state_path) && output_string (")->") &&
                        output_string ("fetch__") && output_sequence (type_begin, type_end) && output_string (");\n") &&
                        output_string ("    const struct ") && output_sequence (type_begin, type_end) &&
                        output_string (" *") && output_string (name) &&
                        output_string (" = kan_repository_event_read_access_resolve (&") && output_string (name) &&
                        output_string ("_access);\n    kan_bool_t ") && output_string (name) &&
                        output_string ("_access_expired = KAN_FALSE;\n    if (") && output_string (name) &&
                        output_string (")\n") && output_use_source_line ();

    if (!output)
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_block_enter (void)
{
    ++process.blocks;
    if (!output_string ("{"))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline kan_bool_t output_singleton_close_access_unguarded (kan_interned_string_t name, const char *access)
{
    return output_string ("if (!") && output_string (name) &&
           output_string ("_access_expired)\n{\n    kan_repository_singleton_") && output_string (access) &&
           output_string ("_access_close (") && output_string (name) && output_string ("_access);\n}\n");
}

static inline kan_bool_t output_indexed_insert_submit_unguarded (kan_interned_string_t name)
{
    return output_string ("kan_repository_indexed_insertion_package_submit (&") && output_string (name) &&
           output_string ("_package);\n");
}

static inline kan_bool_t output_indexed_close_access_unguarded (kan_interned_string_t name,
                                                                const char *query_type,
                                                                const char *access)
{
    return output_string ("        if (!") && output_string (name) &&
           output_string ("_access_expired)\n        {\n            kan_repository_indexed_") &&
           output_string (query_type) && output_string ("_") && output_string (access) &&
           output_string ("_access_close (&") && output_string (name) && output_string ("_access);\n        }\n");
}

static inline kan_bool_t output_indexed_close_cursor_unguarded (kan_interned_string_t name,
                                                                const char *query_type,
                                                                const char *direction_drop_in,
                                                                const char *access)
{
    return output_string ("        kan_repository_indexed_") && output_string (query_type) && output_string ("_") &&
           output_string (direction_drop_in) && output_string (access) && output_string ("_cursor_close (&") &&
           output_string (name) && output_string ("_cursor);\n");
}

static inline kan_bool_t output_indexed_end (kan_interned_string_t name,
                                             const char *query_type,
                                             const char *direction_drop_in,
                                             const char *access)
{
    return output_use_output_line () && output_string ("    else\n    {\n") &&
           output_indexed_close_cursor_unguarded (name, query_type, direction_drop_in, access) &&
           output_string ("        break;\n    }\n}\n") && output_use_source_line ();
}

static inline kan_bool_t output_event_insert_submit_unguarded (kan_interned_string_t name)
{
    return output_use_output_line () && output_string ("        kan_repository_event_insertion_package_submit (&") &&
           output_string (name) && output_string ("_package);\n") && output_use_source_line ();
}

static inline kan_bool_t output_event_close_access_unguarded (kan_interned_string_t name)
{
    return output_string ("        if (!") && output_string (name) &&
           output_string ("_access_expired)\n        {\n            kan_repository_event_read_access_close (&") &&
           output_string (name) && output_string ("_access);\n        }\n");
}

static inline kan_bool_t output_event_end (void)
{
    return output_use_output_line () && output_string ("    else\n    {\n        break;\n    }\n}\n") &&
           output_use_source_line ();
}

static inline enum parse_response_t process_block_exit (void)
{
    --process.blocks;
    if (process.stack_top && process.stack_top->blocks > process.blocks)
    {
        fprintf (stderr,
                 "Error. [%ld:%ld]: Caught blocks exit without block enter after query declaration. Queries should be "
                 "followed by { ... } blocks.\n",
                 (long) io.cursor_line, (long) io.cursor_symbol);
        return PARSE_RESPONSE_FAILED;
    }

    kan_bool_t any_queries = KAN_FALSE;
    while (process.stack_top && process.stack_top->blocks == process.blocks)
    {
        any_queries = KAN_TRUE;
        struct process_query_stack_node_t *previous = process.stack_top->previous;
        const kan_bool_t shared_block = previous && previous->blocks == process.blocks;

        if (shared_block && ((previous->query_type != PROCESS_QUERY_TYPE_SINGLETON_READ &&
                              previous->query_type != PROCESS_QUERY_TYPE_SINGLETON_WRITE) ||
                             (process.stack_top->query_type != PROCESS_QUERY_TYPE_SINGLETON_READ &&
                              process.stack_top->query_type != PROCESS_QUERY_TYPE_SINGLETON_WRITE)))
        {
            fprintf (stderr,
                     "Error. [%ld:%ld]: Caught multiple queries that are not singletons at block exit. Only singleton "
                     "queries are allowed to share block.\n",
                     (long) io.cursor_line, (long) io.cursor_symbol);
            return PARSE_RESPONSE_FAILED;
        }

        switch (process.stack_top->query_type)
        {
        case PROCESS_QUERY_TYPE_SINGLETON_READ:
            if (!output_use_output_line () ||
                !output_singleton_close_access_unguarded (process.stack_top->name, "read") ||
                !output_use_source_line () || (!shared_block && !output_sequence (io.token, io.cursor)))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_SINGLETON_WRITE:
            if (!output_use_output_line () ||
                !output_singleton_close_access_unguarded (process.stack_top->name, "write") ||
                !output_use_source_line () || (!shared_block && !output_sequence (io.token, io.cursor)))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_INDEXED_INSERT:
            if (!output_use_output_line () || !output_indexed_insert_submit_unguarded (process.stack_top->name) ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_SEQUENCE_READ:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "sequence", "read") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "sequence", "", "read"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_SEQUENCE_UPDATE:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "sequence", "update") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "sequence", "", "update"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_SEQUENCE_DELETE:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "sequence", "delete") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "sequence", "", "delete"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_SEQUENCE_WRITE:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "sequence", "write") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "sequence", "", "write"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_VALUE_READ:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "value", "read") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "value", "", "read"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_VALUE_UPDATE:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "value", "update") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "value", "", "update"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_VALUE_DELETE:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "value", "delete") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "value", "", "delete"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_VALUE_WRITE:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "value", "write") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "value", "", "write"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_SIGNAL_READ:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "signal", "read") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "signal", "", "read"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_SIGNAL_UPDATE:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "signal", "update") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "signal", "", "update"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_SIGNAL_DELETE:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "signal", "delete") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "signal", "", "delete"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_SIGNAL_WRITE:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "signal", "write") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "signal", "", "write"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_READ:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "interval", "read") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "interval", "ascending_", "read"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_UPDATE:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "interval", "update") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "interval", "ascending_", "update"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_DELETE:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "interval", "delete") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "interval", "ascending_", "delete"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_WRITE:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "interval", "write") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "interval", "ascending_", "write"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_READ:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "interval", "read") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "interval", "descending_", "read"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_UPDATE:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "interval", "update") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "interval", "descending_", "update"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_DELETE:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "interval", "delete") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "interval", "descending_", "delete"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_WRITE:
            if (!output_use_output_line () ||
                !output_indexed_close_access_unguarded (process.stack_top->name, "interval", "write") ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) ||
                !output_indexed_end (process.stack_top->name, "interval", "descending_", "write"))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_EVENT_INSERT:
            if (!output_use_output_line () || !output_event_insert_submit_unguarded (process.stack_top->name) ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor))
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;

        case PROCESS_QUERY_TYPE_EVENT_FETCH:
            if (!output_use_output_line () || !output_event_close_access_unguarded (process.stack_top->name) ||
                !output_use_source_line () || !output_sequence (io.token, io.cursor) || !output_event_end ())
            {
                fprintf (stderr, "Failure during output.\n");
                return PARSE_RESPONSE_FAILED;
            }

            break;
        }

        kan_free_batched (process.allocation_group, process.stack_top);
        process.stack_top = previous;
    }

    if (!any_queries && !output_sequence (io.token, io.cursor))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline kan_bool_t output_query_access_close_unguarded (struct process_query_stack_node_t *query_node)
{
    switch (query_node->query_type)
    {
    case PROCESS_QUERY_TYPE_SINGLETON_READ:
        return output_singleton_close_access_unguarded (query_node->name, "read");

    case PROCESS_QUERY_TYPE_SINGLETON_WRITE:
        return output_singleton_close_access_unguarded (query_node->name, "write");

    case PROCESS_QUERY_TYPE_INDEXED_INSERT:
        return output_indexed_insert_submit_unguarded (query_node->name);

    case PROCESS_QUERY_TYPE_SEQUENCE_READ:
        return output_indexed_close_access_unguarded (query_node->name, "sequence", "read");

    case PROCESS_QUERY_TYPE_SEQUENCE_UPDATE:
        return output_indexed_close_access_unguarded (query_node->name, "sequence", "update");

    case PROCESS_QUERY_TYPE_SEQUENCE_DELETE:
        return output_indexed_close_access_unguarded (query_node->name, "sequence", "delete");

    case PROCESS_QUERY_TYPE_SEQUENCE_WRITE:
        return output_indexed_close_access_unguarded (query_node->name, "sequence", "write");

    case PROCESS_QUERY_TYPE_VALUE_READ:
        return output_indexed_close_access_unguarded (query_node->name, "value", "read");

    case PROCESS_QUERY_TYPE_VALUE_UPDATE:
        return output_indexed_close_access_unguarded (query_node->name, "value", "update");

    case PROCESS_QUERY_TYPE_VALUE_DELETE:
        return output_indexed_close_access_unguarded (query_node->name, "value", "delete");

    case PROCESS_QUERY_TYPE_VALUE_WRITE:
        return output_indexed_close_access_unguarded (query_node->name, "value", "write");

    case PROCESS_QUERY_TYPE_SIGNAL_READ:
        return output_indexed_close_access_unguarded (query_node->name, "signal", "read");

    case PROCESS_QUERY_TYPE_SIGNAL_UPDATE:
        return output_indexed_close_access_unguarded (query_node->name, "signal", "update");

    case PROCESS_QUERY_TYPE_SIGNAL_DELETE:
        return output_indexed_close_access_unguarded (query_node->name, "signal", "delete");

    case PROCESS_QUERY_TYPE_SIGNAL_WRITE:
        return output_indexed_close_access_unguarded (query_node->name, "signal", "write");

    case PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_READ:
    case PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_READ:
        return output_indexed_close_access_unguarded (query_node->name, "interval", "read");

    case PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_UPDATE:
    case PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_UPDATE:
        return output_indexed_close_access_unguarded (query_node->name, "interval", "update");

    case PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_DELETE:
    case PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_DELETE:
        return output_indexed_close_access_unguarded (query_node->name, "interval", "delete");

    case PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_WRITE:
    case PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_WRITE:
        return output_indexed_close_access_unguarded (query_node->name, "interval", "write");

    case PROCESS_QUERY_TYPE_EVENT_INSERT:
        return output_event_insert_submit_unguarded (query_node->name);

    case PROCESS_QUERY_TYPE_EVENT_FETCH:
        return output_event_close_access_unguarded (query_node->name);
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t output_query_cursor_close_unguarded (struct process_query_stack_node_t *query_node)
{
    switch (query_node->query_type)
    {
    case PROCESS_QUERY_TYPE_SINGLETON_READ:
    case PROCESS_QUERY_TYPE_SINGLETON_WRITE:
    case PROCESS_QUERY_TYPE_INDEXED_INSERT:
    case PROCESS_QUERY_TYPE_EVENT_INSERT:
    case PROCESS_QUERY_TYPE_EVENT_FETCH:
        return KAN_TRUE;

    case PROCESS_QUERY_TYPE_SEQUENCE_READ:
        return output_indexed_close_cursor_unguarded (query_node->name, "sequence", "", "read");

    case PROCESS_QUERY_TYPE_SEQUENCE_UPDATE:
        return output_indexed_close_cursor_unguarded (query_node->name, "sequence", "", "update");

    case PROCESS_QUERY_TYPE_SEQUENCE_DELETE:
        return output_indexed_close_cursor_unguarded (query_node->name, "sequence", "", "delete");

    case PROCESS_QUERY_TYPE_SEQUENCE_WRITE:
        return output_indexed_close_cursor_unguarded (query_node->name, "sequence", "", "write");

    case PROCESS_QUERY_TYPE_VALUE_READ:
        return output_indexed_close_cursor_unguarded (query_node->name, "value", "", "read");

    case PROCESS_QUERY_TYPE_VALUE_UPDATE:
        return output_indexed_close_cursor_unguarded (query_node->name, "value", "", "update");

    case PROCESS_QUERY_TYPE_VALUE_DELETE:
        return output_indexed_close_cursor_unguarded (query_node->name, "value", "", "delete");

    case PROCESS_QUERY_TYPE_VALUE_WRITE:
        return output_indexed_close_cursor_unguarded (query_node->name, "value", "", "write");

    case PROCESS_QUERY_TYPE_SIGNAL_READ:
        return output_indexed_close_cursor_unguarded (query_node->name, "signal", "", "read");

    case PROCESS_QUERY_TYPE_SIGNAL_UPDATE:
        return output_indexed_close_cursor_unguarded (query_node->name, "signal", "", "update");

    case PROCESS_QUERY_TYPE_SIGNAL_DELETE:
        return output_indexed_close_cursor_unguarded (query_node->name, "signal", "", "delete");

    case PROCESS_QUERY_TYPE_SIGNAL_WRITE:
        return output_indexed_close_cursor_unguarded (query_node->name, "signal", "", "write");

    case PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_READ:
        return output_indexed_close_cursor_unguarded (query_node->name, "interval", "ascending_", "read");

    case PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_UPDATE:
        return output_indexed_close_cursor_unguarded (query_node->name, "interval", "ascending_", "update");

    case PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_DELETE:
        return output_indexed_close_cursor_unguarded (query_node->name, "interval", "ascending_", "delete");

    case PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_WRITE:
        return output_indexed_close_cursor_unguarded (query_node->name, "interval", "ascending_", "write");

    case PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_READ:
        return output_indexed_close_cursor_unguarded (query_node->name, "interval", "descending_", "read");

    case PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_UPDATE:
        return output_indexed_close_cursor_unguarded (query_node->name, "interval", "descending_", "update");

    case PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_DELETE:
        return output_indexed_close_cursor_unguarded (query_node->name, "interval", "descending_", "delete");

    case PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_WRITE:
        return output_indexed_close_cursor_unguarded (query_node->name, "interval", "descending_", "write");
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline enum parse_response_t process_query_break (void)
{
    if (!output_markup_macro_comment () || !output_use_output_line ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    if (process.stack_top)
    {
        if (process.stack_top->query_type == PROCESS_QUERY_TYPE_SINGLETON_READ ||
            process.stack_top->query_type == PROCESS_QUERY_TYPE_SINGLETON_WRITE)
        {
            fprintf (
                stderr,
                "Error. [%ld:%ld]: Caught attempt to use break while top query is singleton, which is not supported.\n",
                (long) io.cursor_line, (long) io.cursor_symbol);
            return PARSE_RESPONSE_FAILED;
        }

        if (process.stack_top->query_type == PROCESS_QUERY_TYPE_INDEXED_INSERT ||
            process.stack_top->query_type == PROCESS_QUERY_TYPE_EVENT_INSERT)
        {
            fprintf (
                stderr,
                "Error. [%ld:%ld]: Caught attempt to use break while top query is insert, which is not supported.\n",
                (long) io.cursor_line, (long) io.cursor_symbol);
            return PARSE_RESPONSE_FAILED;
        }

        if (!output_query_access_close_unguarded (process.stack_top) ||
            !output_query_cursor_close_unguarded (process.stack_top))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    if (!output_string ("        break;\n") || !output_use_source_line ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_query_continue (void)
{
    if (!output_markup_macro_comment () || !output_use_output_line ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    if (process.stack_top)
    {
        if (process.stack_top->query_type == PROCESS_QUERY_TYPE_SINGLETON_READ ||
            process.stack_top->query_type == PROCESS_QUERY_TYPE_SINGLETON_WRITE)
        {
            fprintf (stderr,
                     "Error. [%ld:%ld]: Caught attempt to use continue while top query is singleton, which is not "
                     "supported.\n",
                     (long) io.cursor_line, (long) io.cursor_symbol);
            return PARSE_RESPONSE_FAILED;
        }

        if (process.stack_top->query_type == PROCESS_QUERY_TYPE_INDEXED_INSERT ||
            process.stack_top->query_type == PROCESS_QUERY_TYPE_EVENT_INSERT)
        {
            fprintf (
                stderr,
                "Error. [%ld:%ld]: Caught attempt to use continue while top query is insert, which is not supported.\n",
                (long) io.cursor_line, (long) io.cursor_symbol);
            return PARSE_RESPONSE_FAILED;
        }

        if (!output_query_access_close_unguarded (process.stack_top) ||
            !output_query_cursor_close_unguarded (process.stack_top))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }
    }

    if (!output_string ("        continue;\n") || !output_use_source_line ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline kan_bool_t output_close_stack_unguarded (void)
{
    struct process_query_stack_node_t *node = process.stack_top;
    while (node)
    {
        if (!output_query_access_close_unguarded (node) || !output_query_cursor_close_unguarded (node))
        {
            return KAN_FALSE;
        }

        node = node->previous;
    }

    return KAN_TRUE;
}

static inline enum parse_response_t process_query_return_void (void)
{
    if (!output_markup_macro_comment () || !output_use_output_line () || !output_close_stack_unguarded () ||
        !output_string ("        return;\n") || !output_use_source_line ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_mutator_return (void)
{
    if (!output_markup_macro_comment () || !output_use_output_line () || !output_close_stack_unguarded () ||
        !output_string ("        kan_cpu_job_release (job);\n        return;\n") || !output_use_source_line ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_query_return_value (const char *type_begin,
                                                                const char *type_end,
                                                                const char *argument_begin,
                                                                const char *argument_end)
{
    if (!output_markup_macro_comment () || !output_use_output_line () || !output_string ("        ") ||
        !output_sequence (type_begin, type_end) || !output_string (" query_return_value = ") ||
        !output_sequence (argument_begin, argument_end) || !output_string (";\n") || !output_close_stack_unguarded () ||
        !output_string ("        return query_return_value;\n") || !output_use_source_line ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_access_escape (const char *argument_begin,
                                                           const char *argument_end,
                                                           const char *name_begin,
                                                           const char *name_end)
{
    if (!output_markup_macro_comment () || !output_use_output_line () || !output_string ("        ") ||
        !output_sequence (argument_begin, argument_end) || !output_string (" = ") ||
        !output_sequence (name_begin, name_end) || !output_string ("_access;\n        ") ||
        !output_sequence (name_begin, name_end) || !output_string ("_access_expired = KAN_TRUE;\n") ||
        !output_use_source_line ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
}

static inline enum parse_response_t process_access_delete (const char *name_begin, const char *name_end)
{
    if (!output_markup_macro_comment () || !output_use_output_line () || !output_string ("        "))
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    struct process_query_stack_node_t *node = process.stack_top;

    while (node)
    {
        if (node->name == name)
        {
            break;
        }

        node = node->previous;
    }

    if (!node)
    {
        fprintf (
            stderr,
            "Error. [%ld:%ld]: Caught attempt to delete access for query \"%s\", but it cannot be found on stack.\n",
            (long) io.cursor_line, (long) io.cursor_symbol, name);
        return PARSE_RESPONSE_FAILED;
    }

    switch (node->query_type)
    {
    case PROCESS_QUERY_TYPE_SINGLETON_READ:
    case PROCESS_QUERY_TYPE_SINGLETON_WRITE:
    case PROCESS_QUERY_TYPE_INDEXED_INSERT:
    case PROCESS_QUERY_TYPE_SEQUENCE_READ:
    case PROCESS_QUERY_TYPE_SEQUENCE_UPDATE:
    case PROCESS_QUERY_TYPE_VALUE_READ:
    case PROCESS_QUERY_TYPE_VALUE_UPDATE:
    case PROCESS_QUERY_TYPE_SIGNAL_READ:
    case PROCESS_QUERY_TYPE_SIGNAL_UPDATE:
    case PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_READ:
    case PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_UPDATE:
    case PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_READ:
    case PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_UPDATE:
    case PROCESS_QUERY_TYPE_EVENT_INSERT:
    case PROCESS_QUERY_TYPE_EVENT_FETCH:
        fprintf (stderr,
                 "Error. [%ld:%ld]: Caught attempt to delete access for query \"%s\", but query type does not support "
                 "deletion.\n",
                 (long) io.cursor_line, (long) io.cursor_symbol, name);
        return PARSE_RESPONSE_FAILED;

    case PROCESS_QUERY_TYPE_SEQUENCE_DELETE:
        if (!output_string ("kan_repository_indexed_sequence_delete_access_delete (&") || !output_string (name) ||
            !output_string ("_access);\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }

        break;

    case PROCESS_QUERY_TYPE_SEQUENCE_WRITE:
        if (!output_string ("kan_repository_indexed_sequence_write_access_delete (&") || !output_string (name) ||
            !output_string ("_access);\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }

        break;

    case PROCESS_QUERY_TYPE_VALUE_DELETE:
        if (!output_string ("kan_repository_indexed_value_delete_access_delete (&") || !output_string (name) ||
            !output_string ("_access);\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }

        break;

    case PROCESS_QUERY_TYPE_VALUE_WRITE:
        if (!output_string ("kan_repository_indexed_value_write_access_delete (&") || !output_string (name) ||
            !output_string ("_access);\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }

        break;

    case PROCESS_QUERY_TYPE_SIGNAL_DELETE:
        if (!output_string ("kan_repository_indexed_signal_delete_access_delete (&") || !output_string (name) ||
            !output_string ("_access);\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }

        break;

    case PROCESS_QUERY_TYPE_SIGNAL_WRITE:
        if (!output_string ("kan_repository_indexed_signal_write_access_delete (&") || !output_string (name) ||
            !output_string ("_access);\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }

        break;

    case PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_DELETE:
    case PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_DELETE:
        if (!output_string ("kan_repository_indexed_interval_delete_access_delete (&") || !output_string (name) ||
            !output_string ("_access);\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }

        break;

    case PROCESS_QUERY_TYPE_INTERVAL_ASCENDING_WRITE:
    case PROCESS_QUERY_TYPE_INTERVAL_DESCENDING_WRITE:
        if (!output_string ("kan_repository_indexed_interval_write_access_delete (&") || !output_string (name) ||
            !output_string ("_access);\n"))
        {
            fprintf (stderr, "Failure during output.\n");
            return PARSE_RESPONSE_FAILED;
        }

        break;
    }

    if (!output_string ("        ") || !output_sequence (name_begin, name_end) ||
        !output_string ("_access_expired = KAN_TRUE;\n") || !output_use_source_line ())
    {
        fprintf (stderr, "Failure during output.\n");
        return PARSE_RESPONSE_FAILED;
    }

    return PARSE_RESPONSE_BLOCK_PROCESSED;
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
 re2c:define:YYSHIFT = "io.cursor += @@{shift};";
 re2c:define:YYFILL   = "io_refill_buffer () == 0";
 re2c:define:YYSTAGP = "@@{tag} = io.cursor;";
 re2c:define:YYSTAGN = "@@{tag} = NULL;";
 re2c:define:YYSHIFTSTAG  = "@@{tag} += @@{shift};";
 re2c:eof = 0;
 re2c:tags = 1;
 re2c:tags:expression = "io.tags.@@";
*/

// Define common matches.
/*!re2c
 separator = [\x20\x0c\x0a\x0d\x09\x0b];
 any_preprocessor = "#" ((.+) | (.*"\\\n") | (.*"\\\r\n"))*;
 comments = ("//".+) | ("/""*" (((. \ [\x2a]) | "\n")+ "*" (. \ [\x2f]))* ((. \ [\x2a]) | "\n")+ "*""/");
 */

// Define common rules.
/*!rules:re2c:error_on_unknown
 *
 {
     fprintf (stderr, "Error. [%ld:%ld]: Unable to parse next token. Parser: %s. Symbol code: 0x%x.\n",
              (long) io.cursor_line, (long) io.cursor_symbol, __func__, (int) *io.cursor);
     return PARSE_RESPONSE_FAILED;
 }
*/

// Scan phase.

static enum parse_response_t scan_phase_main (void);
static enum parse_response_t scan_phase_generate_state_queries (void);
static enum parse_response_t scan_phase_bind_state (void);
static enum parse_response_t scan_phase_singleton (enum singleton_access_type_t access_type);
static enum parse_response_t scan_phase_indexed_insert (void);
static enum parse_response_t scan_phase_sequence (enum indexed_access_type_t access_type);
static enum parse_response_t scan_phase_value (enum indexed_access_type_t access_type);
static enum parse_response_t scan_phase_signal (enum indexed_access_type_t access_type);
static enum parse_response_t scan_phase_interval (enum indexed_access_type_t access_type);
static enum parse_response_t scan_phase_event_insert (void);
static enum parse_response_t scan_phase_event_fetch (void);

static enum parse_response_t scan_phase_main (void)
{
    while (KAN_TRUE)
    {
        io.token = io.cursor;
        /*!re2c
         "KAN_UP_GENERATE_STATE_QUERIES" separator* "(" { return scan_phase_generate_state_queries (); }
         "KAN_UP_BIND_STATE" separator* "(" { return scan_phase_bind_state (); }
         "KAN_UP_SINGLETON_READ" separator* "(" { return scan_phase_singleton (SINGLETON_ACCESS_TYPE_READ); }
         "KAN_UP_SINGLETON_WRITE" separator* "(" { return scan_phase_singleton (SINGLETON_ACCESS_TYPE_WRITE); }
         "KAN_UP_INDEXED_INSERT" separator* "(" { return scan_phase_indexed_insert (); }
         "KAN_UP_SEQUENCE_READ" separator* "(" { return scan_phase_sequence (INDEXED_ACCESS_TYPE_READ); }
         "KAN_UP_SEQUENCE_UPDATE" separator* "(" { return scan_phase_sequence (INDEXED_ACCESS_TYPE_UPDATE); }
         "KAN_UP_SEQUENCE_DELETE" separator* "(" { return scan_phase_sequence (INDEXED_ACCESS_TYPE_DELETE); }
         "KAN_UP_SEQUENCE_WRITE" separator* "(" { return scan_phase_sequence (INDEXED_ACCESS_TYPE_WRITE); }
         "KAN_UP_VALUE_READ" separator* "(" { return scan_phase_value (INDEXED_ACCESS_TYPE_READ); }
         "KAN_UP_VALUE_UPDATE" separator* "(" { return scan_phase_value (INDEXED_ACCESS_TYPE_UPDATE); }
         "KAN_UP_VALUE_DELETE" separator* "(" { return scan_phase_value (INDEXED_ACCESS_TYPE_DELETE); }
         "KAN_UP_VALUE_WRITE" separator* "(" { return scan_phase_value (INDEXED_ACCESS_TYPE_WRITE); }
         "KAN_UP_SIGNAL_READ" separator* "(" { return scan_phase_signal (INDEXED_ACCESS_TYPE_READ); }
         "KAN_UP_SIGNAL_UPDATE" separator* "(" { return scan_phase_signal (INDEXED_ACCESS_TYPE_UPDATE); }
         "KAN_UP_SIGNAL_DELETE" separator* "(" { return scan_phase_signal (INDEXED_ACCESS_TYPE_DELETE); }
         "KAN_UP_SIGNAL_WRITE" separator* "(" { return scan_phase_signal (INDEXED_ACCESS_TYPE_WRITE); }
         "KAN_UP_INTERVAL_ASCENDING_READ" separator* "(" { return scan_phase_interval (INDEXED_ACCESS_TYPE_READ); }
         "KAN_UP_INTERVAL_ASCENDING_UPDATE" separator* "(" { return scan_phase_interval (INDEXED_ACCESS_TYPE_UPDATE); }
         "KAN_UP_INTERVAL_ASCENDING_DELETE" separator* "(" { return scan_phase_interval (INDEXED_ACCESS_TYPE_DELETE); }
         "KAN_UP_INTERVAL_ASCENDING_WRITE" separator* "(" { return scan_phase_interval (INDEXED_ACCESS_TYPE_WRITE); }
         "KAN_UP_INTERVAL_DESCENDING_READ" separator* "(" { return scan_phase_interval (INDEXED_ACCESS_TYPE_READ); }
         "KAN_UP_INTERVAL_DESCENDING_UPDATE" separator* "(" { return scan_phase_interval (INDEXED_ACCESS_TYPE_UPDATE); }
         "KAN_UP_INTERVAL_DESCENDING_DELETE" separator* "(" { return scan_phase_interval (INDEXED_ACCESS_TYPE_DELETE); }
         "KAN_UP_INTERVAL_DESCENDING_WRITE" separator* "(" { return scan_phase_interval (INDEXED_ACCESS_TYPE_WRITE); }
         "KAN_UP_EVENT_INSERT" separator* "(" { return scan_phase_event_insert (); }
         "KAN_UP_EVENT_FETCH" separator* "(" { return scan_phase_event_fetch (); }

         "KAN_UP_QUERY_BREAK" { continue; }
         "KAN_UP_QUERY_CONTINUE" { continue; }
         "KAN_UP_QUERY_RETURN_VOID" { continue; }
         "KAN_UP_MUTATOR_RETURN" { continue; }
         "KAN_UP_QUERY_RETURN_VALUE" { continue; }
         "KAN_UP_ACCESS_ESCAPE" { continue; }
         "KAN_UP_ACCESS_DELETE" { continue; }

         "KAN_UP_" [A-Za-z_]+
         {
             fprintf (stderr, "Error. [%ld:%ld]: Unknown preprocessor macro \"%s\".\n",
                 (long) io.cursor_line, (long) io.cursor_symbol, kan_char_sequence_intern (io.token, io.cursor));
             return PARSE_RESPONSE_FAILED;
         }

         comments { continue; }
         * { continue; }
         $ { return PARSE_RESPONSE_FINISHED; }
         */
    }
}

static enum parse_response_t scan_phase_generate_state_queries (void)
{
    while (KAN_TRUE)
    {
        const char *name_begin;
        const char *name_end;

        io.token = io.cursor;
        /*!re2c
         !use:error_on_unknown;

         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ")"
         {
             return scan_generate_state_queries (name_begin, name_end) ?
                 PARSE_RESPONSE_BLOCK_PROCESSED : PARSE_RESPONSE_FAILED;
         }

         $
         {
             fprintf (stderr, "Error. Reached end of file while scanning bind state macro.");
             return PARSE_RESPONSE_FAILED;
         }
         */
    }
}

static enum parse_response_t scan_phase_bind_state (void)
{
    while (KAN_TRUE)
    {
        const char *name_begin;
        const char *name_end;
        const char *path_begin;
        const char *path_end;

        io.token = io.cursor;
        /*!re2c
         !use:error_on_unknown;

         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @path_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @path_end separator* ")"
         {
             return scan_bind_state (name_begin, name_end, path_begin, path_end) ?
                 PARSE_RESPONSE_BLOCK_PROCESSED : PARSE_RESPONSE_FAILED;
         }

         $
         {
             fprintf (stderr, "Error. Reached end of file while scanning bind state macro.");
             return PARSE_RESPONSE_FAILED;
         }
         */
    }
}

static enum parse_response_t scan_phase_singleton (enum singleton_access_type_t access_type)
{
    while (KAN_TRUE)
    {
        const char *name_begin;
        const char *name_end;
        const char *type_begin;
        const char *type_end;

        io.token = io.cursor;
        /*!re2c
         !use:error_on_unknown;

         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ")"
         {
             return scan_singleton (name_begin, name_end, access_type, type_begin, type_end) ?
                 PARSE_RESPONSE_BLOCK_PROCESSED : PARSE_RESPONSE_FAILED;
         }

         $
         {
             fprintf (stderr, "Error. Reached end of file while scanning singleton macro.");
             return PARSE_RESPONSE_FAILED;
         }
         */
    }
}

static enum parse_response_t scan_phase_indexed_insert (void)
{
    while (KAN_TRUE)
    {
        const char *name_begin;
        const char *name_end;
        const char *type_begin;
        const char *type_end;

        io.token = io.cursor;
        /*!re2c
         !use:error_on_unknown;

        separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
        separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ")"
        separator* "{"
        {
            return scan_indexed_insert (name_begin, name_end, type_begin, type_end) ?
                       PARSE_RESPONSE_BLOCK_PROCESSED : PARSE_RESPONSE_FAILED;
        }

        $
        {
            fprintf (stderr, "Error. Reached end of file while scanning indexed insert macro.");
            return PARSE_RESPONSE_FAILED;
        }
        */
    }
}

static enum parse_response_t scan_phase_sequence (enum indexed_access_type_t access_type)
{
    while (KAN_TRUE)
    {
        const char *name_begin;
        const char *name_end;
        const char *type_begin;
        const char *type_end;

        io.token = io.cursor;
        /*!re2c
         !use:error_on_unknown;

         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* separator* ")"
         separator* "{"
         {
             return scan_sequence (name_begin, name_end, access_type, type_begin, type_end) ?
                 PARSE_RESPONSE_BLOCK_PROCESSED : PARSE_RESPONSE_FAILED;
         }

         $
         {
             fprintf (stderr, "Error. Reached end of file while scanning sequence macro.");
             return PARSE_RESPONSE_FAILED;
         }
         */
    }
}

static enum parse_response_t scan_phase_value (enum indexed_access_type_t access_type)
{
    while (KAN_TRUE)
    {
        const char *name_begin;
        const char *name_end;
        const char *type_begin;
        const char *type_end;
        const char *field_begin;
        const char *field_end;
        const char *argument_begin;
        const char *argument_end;

        io.token = io.cursor;
        /*!re2c
         !use:error_on_unknown;

         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_end separator* ")"
         separator* "{"
         {
             return scan_value (name_begin, name_end, access_type, type_begin, type_end, field_begin, field_end,
                 argument_begin, argument_end) ?
                 PARSE_RESPONSE_BLOCK_PROCESSED : PARSE_RESPONSE_FAILED;
         }

         $
         {
             fprintf (stderr, "Error. Reached end of file while scanning indexed insert macro.");
             return PARSE_RESPONSE_FAILED;
         }
         */
    }
}

static enum parse_response_t scan_phase_signal (enum indexed_access_type_t access_type)
{
    while (KAN_TRUE)
    {
        const char *name_begin;
        const char *name_end;
        const char *type_begin;
        const char *type_end;
        const char *field_begin;
        const char *field_end;
        const char *value_begin;
        const char *value_end;

        io.token = io.cursor;
        /*!re2c
         !use:error_on_unknown;

         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @value_begin [0-9]+ @value_end separator* ")"
         separator* "{"
         {
             return scan_signal (name_begin, name_end, access_type, type_begin, type_end, field_begin, field_end,
                 value_begin, value_end) ?
                 PARSE_RESPONSE_BLOCK_PROCESSED : PARSE_RESPONSE_FAILED;
         }

         $
         {
             fprintf (stderr, "Error. Reached end of file while scanning indexed insert macro.");
             return PARSE_RESPONSE_FAILED;
         }
         */
    }
}

static enum parse_response_t scan_phase_interval (enum indexed_access_type_t access_type)
{
    while (KAN_TRUE)
    {
        const char *name_begin;
        const char *name_end;
        const char *type_begin;
        const char *type_end;
        const char *field_begin;
        const char *field_end;
        const char *argument_min_begin;
        const char *argument_min_end;
        const char *argument_max_begin;
        const char *argument_max_end;

        io.token = io.cursor;
        /*!re2c
         !use:error_on_unknown;

         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_min_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_min_end separator* ","
         separator* @argument_max_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_max_end separator* ")"
         separator* "{"
         {
             return scan_interval (name_begin, name_end, access_type, type_begin, type_end, field_begin, field_end,
                 argument_min_begin, argument_min_end, argument_max_begin, argument_max_end) ?
                 PARSE_RESPONSE_BLOCK_PROCESSED : PARSE_RESPONSE_FAILED;
         }

         $
         {
             fprintf (stderr, "Error. Reached end of file while scanning indexed insert macro.");
             return PARSE_RESPONSE_FAILED;
         }
         */
    }
}

static enum parse_response_t scan_phase_event_insert (void)
{
    while (KAN_TRUE)
    {
        const char *name_begin;
        const char *name_end;
        const char *type_begin;
        const char *type_end;

        io.token = io.cursor;
        /*!re2c
         !use:error_on_unknown;

         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ")"
         separator* "{"
         {
             return scan_event_insert (name_begin, name_end, type_begin, type_end) ?
                 PARSE_RESPONSE_BLOCK_PROCESSED : PARSE_RESPONSE_FAILED;
         }

         $
         {
             fprintf (stderr, "Error. Reached end of file while scanning event insert macro.");
             return PARSE_RESPONSE_FAILED;
         }
         */
    }
}

static enum parse_response_t scan_phase_event_fetch (void)
{
    while (KAN_TRUE)
    {
        const char *name_begin;
        const char *name_end;
        const char *type_begin;
        const char *type_end;

        io.token = io.cursor;
        /*!re2c
         !use:error_on_unknown;

        separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
        separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ")"
        separator* "{"
        {
            return scan_event_fetch (name_begin, name_end, type_begin, type_end) ?
                       PARSE_RESPONSE_BLOCK_PROCESSED : PARSE_RESPONSE_FAILED;
        }

        $
        {
            fprintf (stderr, "Error. Reached end of file while scanning event insert macro.");
            return PARSE_RESPONSE_FAILED;
        }
        */
    }
}

// Process phase.

static enum parse_response_t process_phase_main (void);

static enum parse_response_t process_phase_main (void)
{
    // We can be a little bit more lazy here with error handling as
    // scan pass already happened and it has checked syntax,

    while (KAN_TRUE)
    {
        const char *name_begin;
        const char *name_end;
        const char *path_begin;
        const char *path_end;
        const char *type_begin;
        const char *type_end;
        const char *field_begin;
        const char *field_end;
        const char *argument_begin;
        const char *argument_end;
        const char *argument_min_begin;
        const char *argument_min_end;
        const char *argument_max_begin;
        const char *argument_max_end;

        io.token = io.cursor;
        /*!re2c
         "KAN_UP_GENERATE_STATE_QUERIES" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ")"
         (" "* "\n")?
         { return process_generate_state_queries (name_begin, name_end); }

         "KAN_UP_BIND_STATE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @path_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @path_end separator* ")"
         (" "* "\n")?
         { return process_bind_state (name_begin, name_end, path_begin, path_end); }

         "KAN_UP_SINGLETON_READ" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ")"
         (" "* "\n")?
         { return process_singleton_read (name_begin, name_end, type_begin, type_end); }

         "KAN_UP_SINGLETON_WRITE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ")"
         (" "* "\n")?
         { return process_singleton_write (name_begin, name_end, type_begin, type_end); }

         "KAN_UP_INDEXED_INSERT" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ")"
         (" "* "\n")?
         { return process_indexed_insert (name_begin, name_end, type_begin, type_end); }

         "KAN_UP_SEQUENCE_READ" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* separator* ")"
         (" "* "\n")?
         { return process_sequence_read (name_begin, name_end, type_begin, type_end); }

         "KAN_UP_SEQUENCE_UPDATE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* separator* ")"
         (" "* "\n")?
         { return process_sequence_update (name_begin, name_end, type_begin, type_end); }

         "KAN_UP_SEQUENCE_DELETE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* separator* ")"
         (" "* "\n")?
         { return process_sequence_delete (name_begin, name_end, type_begin, type_end); }

         "KAN_UP_SEQUENCE_WRITE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* separator* ")"
         (" "* "\n")?
         { return process_sequence_write (name_begin, name_end, type_begin, type_end); }

         "KAN_UP_VALUE_READ" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_end separator* ")"
         (" "* "\n")?
         {
             return process_value_read (name_begin, name_end, type_begin, type_end, field_begin, field_end,
                 argument_begin, argument_end);
         }

         "KAN_UP_VALUE_UPDATE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_end separator* ")"
         (" "* "\n")?
         {
             return process_value_update (name_begin, name_end, type_begin, type_end, field_begin, field_end,
                 argument_begin, argument_end);
         }

         "KAN_UP_VALUE_DELETE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_end separator* ")"
         (" "* "\n")?
         {
             return process_value_delete (name_begin, name_end, type_begin, type_end, field_begin, field_end,
                 argument_begin, argument_end);
         }

         "KAN_UP_VALUE_WRITE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_end separator* ")"
         (" "* "\n")?
         {
             return process_value_write (name_begin, name_end, type_begin, type_end, field_begin, field_end,
                 argument_begin, argument_end);
         }

         "KAN_UP_SIGNAL_READ" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_begin [0-9]+ @argument_end separator* ")"
         (" "* "\n")?
         {
             return process_signal_read (name_begin, name_end, type_begin, type_end, field_begin, field_end,
                 argument_begin, argument_end);
         }

         "KAN_UP_SIGNAL_UPDATE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_begin [0-9]+ @argument_end separator* ")"
         (" "* "\n")?
         {
             return process_signal_update (name_begin, name_end, type_begin, type_end, field_begin, field_end,
                 argument_begin, argument_end);
         }

         "KAN_UP_SIGNAL_DELETE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_begin [0-9]+ @argument_end separator* ")"
         (" "* "\n")?
         {
             return process_signal_delete (name_begin, name_end, type_begin, type_end, field_begin, field_end,
                 argument_begin, argument_end);
         }

         "KAN_UP_SIGNAL_WRITE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_begin [0-9]+ @argument_end separator* ")"
         (" "* "\n")?
         {
             return process_signal_write (name_begin, name_end, type_begin, type_end, field_begin, field_end,
                 argument_begin, argument_end);
         }

         "KAN_UP_INTERVAL_ASCENDING_READ" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_min_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_min_end separator* ","
         separator* @argument_max_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_max_end separator* ")"
         (" "* "\n")?
         {
             return process_interval_read (name_begin, name_end, type_begin, type_end, field_begin, field_end,
                 KAN_TRUE, argument_min_begin, argument_min_end, argument_max_begin, argument_max_end);
         }

         "KAN_UP_INTERVAL_ASCENDING_UPDATE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_min_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_min_end separator* ","
         separator* @argument_max_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_max_end separator* ")"
         (" "* "\n")?
         {
             return process_interval_update (name_begin, name_end, type_begin, type_end, field_begin, field_end,
                 KAN_TRUE, argument_min_begin, argument_min_end, argument_max_begin, argument_max_end);
         }

         "KAN_UP_INTERVAL_ASCENDING_DELETE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_min_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_min_end separator* ","
         separator* @argument_max_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_max_end separator* ")"
         (" "* "\n")?
         {
             return process_interval_delete (name_begin, name_end, type_begin, type_end, field_begin, field_end,
                 KAN_TRUE, argument_min_begin, argument_min_end, argument_max_begin, argument_max_end);
         }

         "KAN_UP_INTERVAL_ASCENDING_WRITE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_min_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_min_end separator* ","
         separator* @argument_max_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_max_end separator* ")"
         (" "* "\n")?
         {
             return process_interval_write (name_begin, name_end, type_begin, type_end, field_begin, field_end,
                 KAN_TRUE, argument_min_begin, argument_min_end, argument_max_begin, argument_max_end);
         }

         "KAN_UP_INTERVAL_DESCENDING_READ" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_min_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_min_end separator* ","
         separator* @argument_max_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_max_end separator* ")"
         (" "* "\n")?
         {
             return process_interval_read (name_begin, name_end, type_begin, type_end, field_begin, field_end,
                 KAN_FALSE, argument_min_begin, argument_min_end, argument_max_begin, argument_max_end);
         }

         "KAN_UP_INTERVAL_DESCENDING_UPDATE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_min_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_min_end separator* ","
         separator* @argument_max_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_max_end separator* ")"
         (" "* "\n")?
         {
             return process_interval_update (name_begin, name_end, type_begin, type_end, field_begin, field_end,
                 KAN_FALSE, argument_min_begin, argument_min_end, argument_max_begin, argument_max_end);
         }

         "KAN_UP_INTERVAL_DESCENDING_DELETE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_min_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_min_end separator* ","
         separator* @argument_max_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_max_end separator* ")"
         (" "* "\n")?
         {
             return process_interval_delete (name_begin, name_end, type_begin, type_end, field_begin, field_end,
                 KAN_FALSE, argument_min_begin, argument_min_end, argument_max_begin, argument_max_end);
         }

         "KAN_UP_INTERVAL_DESCENDING_WRITE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @field_begin ([A-Za-z0-9_] | ".")+ @field_end separator* ","
         separator* @argument_min_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_min_end separator* ","
         separator* @argument_max_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_max_end separator* ")"
         (" "* "\n")?
         {
             return process_interval_write (name_begin, name_end, type_begin, type_end, field_begin, field_end,
                 KAN_FALSE, argument_min_begin, argument_min_end, argument_max_begin, argument_max_end);
         }

         "KAN_UP_EVENT_INSERT" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* separator* ")"
         (" "* "\n")?
         { return process_event_insert (name_begin, name_end, type_begin, type_end); }

         "KAN_UP_EVENT_FETCH" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ","
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* separator* ")"
         (" "* "\n")?
         { return process_event_fetch (name_begin, name_end, type_begin, type_end); }

         "{"
         { return process_block_enter (); }

         " "* "}" (" "* "\n")?
         { return process_block_exit (); }

         "KAN_UP_QUERY_BREAK" separator* ("(" separator* ")")? ";" (" "* "\n")?
         { return process_query_break (); }

         "KAN_UP_QUERY_CONTINUE" separator* ("(" separator* ")")? ";" (" "* "\n")?
         { return process_query_continue (); }

         "KAN_UP_QUERY_RETURN_VOID" separator* ("(" separator* ")")? ";" (" "* "\n")?
         { return process_query_return_void (); }

         "KAN_UP_MUTATOR_RETURN" separator* ("(" separator* ")")? ";" (" "* "\n")?
         { return process_mutator_return (); }

         "KAN_UP_QUERY_RETURN_VALUE" separator* "("
         separator* @type_begin [A-Za-z_][A-Za-z0-9_]* @type_end separator* ","
         separator* @argument_begin ("&" | "*" | [A-Za-z0-9_+-/] | "->" | "." | "," | "(" | ")")+ @argument_end
         separator* ");" (" "* "\n")?
         { return process_query_return_value (type_begin, type_end, argument_begin, argument_end); }

         "KAN_UP_ACCESS_ESCAPE" separator* "("
         separator* @argument_begin ("&" | "*" | [A-Za-z0-9_] | "->" | ".")+ @argument_end separator* ","
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ");"
         (" "* "\n")?
         { return process_access_escape (argument_begin, argument_end, name_begin, name_end); }

         "KAN_UP_ACCESS_DELETE" separator* "("
         separator* @name_begin [A-Za-z_][A-Za-z0-9_]* @name_end separator* ");"
         (" "* "\n")?
         { return process_access_delete (name_begin, name_end); }

         * { output_sequence (io.token, io.cursor); continue; }
         $ { return PARSE_RESPONSE_FINISHED; }
         */
    }
}

// Main routine.

static kan_bool_t perform_scan_phase (void)
{
    enum parse_response_t response;

    while ((response = scan_phase_main ()) == PARSE_RESPONSE_BLOCK_PROCESSED)
    {
    }

    return response == PARSE_RESPONSE_FINISHED;
}

static kan_bool_t perform_process_phase (void)
{
    process.allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "process");
    if (!output_use_source_line ())
    {
        return KAN_FALSE;
    }

    enum parse_response_t response;
    while ((response = process_phase_main ()) == PARSE_RESPONSE_BLOCK_PROCESSED)
    {
    }

    return response == PARSE_RESPONSE_FINISHED;
}

int main (int argument_count, char **arguments_array)
{
    if (argument_count != 3)
    {
        fprintf (stderr,
                 "Unknown arguments. Expected arguments:\n"
                 "- input_file_path: path to input file.\n"
                 "- output_file_path: path to output file.\n");
        return RETURN_CODE_INVALID_ARGUMENTS;
    }

    arguments.input_file_path = arguments_array[1u];
    arguments.output_file_path = arguments_array[2u];

    io.input_stream = kan_direct_file_stream_open_for_read (arguments.input_file_path, KAN_FALSE);
    if (!io.input_stream)
    {
        fprintf (stderr, "Unable to open input file \"%s\".\n", arguments.input_file_path);
        return RETURN_CODE_UNABLE_TO_OPEN_INPUT;
    }

    scan_init ();
    if (!perform_scan_phase ())
    {
        fprintf (stderr, "Scan phase failed, exiting...\n");
        io.input_stream->operations->close (io.input_stream);
        scan_shutdown ();
        return RETURN_CODE_PARSE_FAILED;
    }

    io.input_stream->operations->seek (io.input_stream, KAN_STREAM_SEEK_START, 0u);
    io_reset ();

    io.output_stream = kan_direct_file_stream_open_for_write (arguments.output_file_path, KAN_TRUE);
    if (!io.output_stream)
    {
        fprintf (stderr, "Unable to open output file \"%s\".\n", arguments.output_file_path);
        scan_shutdown ();
        return RETURN_CODE_UNABLE_TO_OPEN_OUTPUT;
    }

    io.output_stream =
        kan_random_access_stream_buffer_open_for_write (io.output_stream, KAN_UNIVERSE_PREPROCESSOR_OUTPUT_BUFFER_SIZE);

    if (!perform_process_phase ())
    {
        fprintf (stderr, "Process phase failed, exiting...\n");
        io.input_stream->operations->close (io.input_stream);
        io.output_stream->operations->close (io.output_stream);
        scan_shutdown ();
        return RETURN_CODE_PROCESS_FAILED;
    }

    io.input_stream->operations->close (io.input_stream);
    io.output_stream->operations->close (io.output_stream);
    scan_shutdown ();
    return RETURN_CODE_SUCCESS;
}
