#pragma once

#include <stddef.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/api_common/highlight.h>
#include <kan/universe/universe.h>

/// \file
/// \brief Provides markup macros for usage with universe preprocessor.
///
/// \par Motivation
/// \parblock
/// Query API is versatile, but it is a bit too verbose in most cases. Verbosity of this API makes it more difficult to
/// write and read mutator code. Therefore, it was decided to introduce special custom preprocessor that uses
/// macro-driven markup to automatically generate verbose code for common use cases.
/// \endparblock
///
/// \par Capabilities
/// \parblock
/// - State query fields are automatically generated from query markup, making it easy to introduce new query to code:
///   just write the queries and required state fields will be autogenerated. Query names follow universe automated
///   lifetime guidelines, therefore they do not require manual deploy and undeploy steps.
///
/// - Simplified query syntax. User no longer needs to manually manage cursors and accesses, only blocks with actual
///   data processing logic are required. For example:
///
///   ```c
///   KAN_UP_SEQUENCE_READ (my_data, my_data_type_t)
///   {
///       // ... your logic here that works on my_data pointer for every instance of my_data_type_t in repository ...
///   }
///   ```
///
/// - Simplified query execution control. User can add macros like `KAN_UP_QUERY_BREAK` to break out of query early
///   or even `KAN_UP_QUERY_RETURN_VALUE` to calculate return value and then close all the accesses and cursors.
///
/// - Specialized access management is still possible through `KAN_UP_ACCESS_ESCAPE` and `KAN_UP_ACCESS_DELETE`.
/// \endparblock
///
/// \par Usage basics
/// \parblock
/// Foundation for query usage is a state: structure that will contain repository query instances. To declare state,
/// use `KAN_UP_GENERATE_STATE_QUERIES` inside any structure you need:
///
/// ```c
/// struct my_mutator_state_t
/// {
///     KAN_UP_GENERATE_STATE_QUERIES (my_mutator);
/// };
/// ```
///
/// Alternatively, there can be states with pre-filled queries (queries created manually, not through preprocessor).
/// State is declared pre-filled when its binding is found before KAN_UP_GENERATE_STATE_QUERIES with this name.
/// In this case, KAN_UP_GENERATE_STATE_QUERIES should never be called for this state.
///
/// State name argument is later used for binding states.
///
/// Binding state means that all queries below (until another binding) are working with the state that has given name
/// and which instance pointer will be available at given state path. If you're using standardized state pointer in
/// every function, which is an advised approach for universe, you can bind state only once for all functions.
///
/// When state is declared and bound, you can use query macros. They are split into several groups:
///
/// - Singleton queries. Their feature is that several singleton queries can share one code block, but they also
///   do not support query break and continue macros. Example:
///
/// ```c
/// KAN_UP_SINGLETON_READ (readonly_singleton, my_first_singleton_type_t)
/// KAN_UP_SINGLETON_WRITE (writable_singleton, my_second_singleton_type_t)
/// {
///     writable_singleton->some_field = readonly_singleton->some_field;
/// }
/// ```
///
/// - Insertion queries. This queries are used for inserting single instance of data, therefore they do not support
///   query break and continue macro. Example:
///
/// ```c
/// KAN_UP_INDEXED_INSERT (new_instance, my_instance_type_t)
/// {
///     new_instance->some_field = some_data;
/// }
/// ```
///
/// - Range queries. These queries operate on several instances of data, executing block for every instance once.
///   They support query break and continue macros. Example:
///
/// ```c
/// KAN_UP_VALUE_READ (instance, my_instance_type_t, key_field, &key_data)
/// {
///     if (instance->other_field == other_data)
///     {
///         found = KAN_TRUE;
///         KAN_UP_QUERY_BREAK;
///     }
/// }
/// ```
/// \endparblock
///
/// \par Limitations
/// \parblock
/// - Space queries are not supported, because it is unclear how to automatically generate them inside state as
///   they require additional meta generation for their state fields, which is out of scope for now.
/// - `KAN_UP_QUERY_BREAK` and `KAN_UP_QUERY_CONTINUE ` should be used with care: they're just break and continue
///   keywords that manage accesses and cursors before executing, therefore using them inside normal `for`, `switch` or
///   `while` will result in undefined behavior.
/// \endparblock
///
/// \par Autocomplete
/// \parblock
/// Macros provided in this file should provide enough information for code highlight and autocomplete features
/// to work in most IDEs.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Use this in queries instead of NULL. As some preprocessors spam line directives when encountering NULL (GCC
///        does that for some reason), we use our own macro to make parsing of kan universe preprocessor macros easier.
#define KAN_UP_NOTHING ((void *) 0)

// Defines are only enabled for highlight. During real compilation, universe preprocessor consumes them.
#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)

/// \brief Declares state with given name and outputs list of query fields associated with this state.
#    define KAN_UP_GENERATE_STATE_QUERIES(STATE_NAME)                                                                  \
        /* Highlight-autocomplete replacement. */                                                                      \
        kan_memory_size_t STATE_NAME##_fake_placeholder_field;

/// \brief Binds state by name with given path for all queries below (until another bind overrides it).
/// \details If state is not declared yet, declares it, making it a pre-filled queries state.
///          KAN_UP_GENERATE_STATE_QUERIES can no longer be called for such state.
#    define KAN_UP_BIND_STATE(STATE_NAME, STATE_PATH) /* No highlight-time replacement. */

/// \brief Closes current query access and cursor and then emits break keyword.
#    define KAN_UP_QUERY_BREAK                                                                                         \
        /* Highlight-autocomplete replacement. */                                                                      \
        break

/// \brief Closes current query access and cursor and then emits continue keyword.
#    define KAN_UP_QUERY_CONTINUE                                                                                      \
        /* Highlight-autocomplete replacement. */                                                                      \
        continue

/// \brief Clothes all accesses and cursors and then returns from the function.
#    define KAN_UP_QUERY_RETURN_VOID                                                                                   \
        /* Highlight-autocomplete replacement. */                                                                      \
        return

/// \brief Clothes all accesses and cursors, releases mutator job and then returns from the mutator execute function.
#    define KAN_UP_MUTATOR_RETURN                                                                                      \
        /* Highlight-autocomplete replacement. */                                                                      \
        kan_cpu_job_release (job);                                                                                     \
        return

/// \brief Calculates return value of given type using given expression, then closes all accesses and cursors and
///        returns calculated value from the function.
#    define KAN_UP_QUERY_RETURN_VALUE(TYPE, ...)                                                                       \
        /* Highlight-autocomplete replacement. */                                                                      \
        TYPE return_value = __VA_ARGS__;                                                                               \
        return return_value

/// \brief Copies current access of query with given name to given target expression and marks access as escaped
///        (will not be closed).
#    define KAN_UP_ACCESS_ESCAPE(TARGET, NAME)                                                                         \
        /* Highlight-autocomplete replacement. */                                                                      \
        TARGET = NAME##_access

/// \brief Deletes data under current access of query with given name and marks access as deleted (will not be closed).
#    define KAN_UP_ACCESS_DELETE(NAME)                                                                                 \
        /* Highlight-autocomplete replacement. */                                                                      \
        NAME = NULL

/// \brief Header for singleton read query.
#    define KAN_UP_SINGLETON_READ(NAME, TYPE)                                                                          \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;

/// \brief Header for singleton write query.
#    define KAN_UP_SINGLETON_WRITE(NAME, TYPE)                                                                         \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;

/// \brief Header for indexed insert query.
#    define KAN_UP_INDEXED_INSERT(NAME, TYPE)                                                                          \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed sequence read query.
#    define KAN_UP_SEQUENCE_READ(NAME, TYPE)                                                                           \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        struct kan_repository_indexed_sequence_read_access_t NAME##_access = {0};                                      \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed sequence update query.
#    define KAN_UP_SEQUENCE_UPDATE(NAME, TYPE)                                                                         \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        struct kan_repository_indexed_sequence_update_access_t NAME##_access = {0};                                    \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed sequence delete query.
#    define KAN_UP_SEQUENCE_DELETE(NAME, TYPE)                                                                         \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        struct kan_repository_indexed_sequence_delete_access_t NAME##_access = {0};                                    \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed sequence write query.
#    define KAN_UP_SEQUENCE_WRITE(NAME, TYPE)                                                                          \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        struct kan_repository_indexed_sequence_write_access_t NAME##_access = {0};                                     \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed value read query.
#    define KAN_UP_VALUE_READ(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                                     \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        const void *NAME##_argument_user = ARGUMENT_POINTER;                                                           \
        struct kan_repository_indexed_value_read_access_t NAME##_access = {0};                                         \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed value update query.
#    define KAN_UP_VALUE_UPDATE(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                                   \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        const void *NAME##_argument_user = ARGUMENT_POINTER;                                                           \
        struct kan_repository_indexed_value_update_access_t NAME##_access = {0};                                       \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed value delete query.
#    define KAN_UP_VALUE_DELETE(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                                   \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        const void *NAME##_argument_user = ARGUMENT_POINTER;                                                           \
        struct kan_repository_indexed_value_delete_access_t NAME##_access = {0};                                       \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed value write query.
#    define KAN_UP_VALUE_WRITE(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                                    \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        const void *NAME##_argument_user = ARGUMENT_POINTER;                                                           \
        struct kan_repository_indexed_value_write_access_t NAME##_access = {0};                                        \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed signal read query.
#    define KAN_UP_SIGNAL_READ(NAME, TYPE, FIELD, NUMERIC_CONSTANT)                                                    \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_signal_read_access_t NAME##_access = {0};                                        \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed signal update query.
#    define KAN_UP_SIGNAL_UPDATE(NAME, TYPE, FIELD, NUMERIC_CONSTANT)                                                  \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_signal_update_access_t NAME##_access = {0};                                      \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed signal delete query.
#    define KAN_UP_SIGNAL_DELETE(NAME, TYPE, FIELD, NUMERIC_CONSTANT)                                                  \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_signal_delete_access_t NAME##_access = {0};                                      \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed signal write query.
#    define KAN_UP_SIGNAL_WRITE(NAME, TYPE, FIELD, NUMERIC_CONSTANT)                                                   \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_signal_write_access_t NAME##_access = {0};                                       \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed interval ascending read query.
#    define KAN_UP_INTERVAL_ASCENDING_READ(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)              \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        const void *NAME##_argument_min_user = ARGUMENT_MIN_POINTER;                                                   \
        const void *NAME##_argument_max_user = ARGUMENT_MAX_POINTER;                                                   \
        struct kan_repository_indexed_interval_read_access_t NAME##_access = {0};                                      \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed interval ascending update query.
#    define KAN_UP_INTERVAL_ASCENDING_UPDATE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)            \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        const void *NAME##_argument_min_user = ARGUMENT_MIN_POINTER;                                                   \
        const void *NAME##_argument_max_user = ARGUMENT_MAX_POINTER;                                                   \
        struct kan_repository_indexed_interval_update_access_t NAME##_access = {0};                                    \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed interval ascending delete query.
#    define KAN_UP_INTERVAL_ASCENDING_DELETE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)            \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        const void *NAME##_argument_min_user = ARGUMENT_MIN_POINTER;                                                   \
        const void *NAME##_argument_max_user = ARGUMENT_MAX_POINTER;                                                   \
        struct kan_repository_indexed_interval_delete_access_t NAME##_access = {0};                                    \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed interval ascending write query.
#    define KAN_UP_INTERVAL_ASCENDING_WRITE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)             \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        const void *NAME##_argument_min_user = ARGUMENT_MIN_POINTER;                                                   \
        const void *NAME##_argument_max_user = ARGUMENT_MAX_POINTER;                                                   \
        struct kan_repository_indexed_interval_write_access_t NAME##_access = {0};                                     \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed interval descending read query.
#    define KAN_UP_INTERVAL_DESCENDING_READ(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)             \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        const void *NAME##_argument_min_user = ARGUMENT_MIN_POINTER;                                                   \
        const void *NAME##_argument_max_user = ARGUMENT_MAX_POINTER;                                                   \
        struct kan_repository_indexed_interval_read_access_t NAME##_access = {0};                                      \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed interval descending update query
#    define KAN_UP_INTERVAL_DESCENDING_UPDATE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)           \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        const void *NAME##_argument_min_user = ARGUMENT_MIN_POINTER;                                                   \
        const void *NAME##_argument_max_user = ARGUMENT_MAX_POINTER;                                                   \
        struct kan_repository_indexed_interval_update_access_t NAME##_access = {0};                                    \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed interval descending delete query
#    define KAN_UP_INTERVAL_DESCENDING_DELETE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)           \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        const void *NAME##_argument_min_user = ARGUMENT_MIN_POINTER;                                                   \
        const void *NAME##_argument_max_user = ARGUMENT_MAX_POINTER;                                                   \
        struct kan_repository_indexed_interval_delete_access_t NAME##_access = {0};                                    \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for indexed interval descending write query
#    define KAN_UP_INTERVAL_DESCENDING_WRITE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)            \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        const void *NAME##_argument_min_user = ARGUMENT_MIN_POINTER;                                                   \
        const void *NAME##_argument_max_user = ARGUMENT_MAX_POINTER;                                                   \
        struct kan_repository_indexed_interval_write_access_t NAME##_access = {0};                                     \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for event insert query.
/// \warning Query block is not executed if event insertion package is empty (that means that there is no readers).
#    define KAN_UP_EVENT_INSERT(NAME, TYPE)                                                                            \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

/// \brief Header for event fetch query.
#    define KAN_UP_EVENT_FETCH(NAME, TYPE)                                                                             \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        for (kan_loop_size_t NAME##_fake_index = 0u; NAME##_fake_index < 1u; ++NAME##_fake_index)

#endif

KAN_C_HEADER_END
