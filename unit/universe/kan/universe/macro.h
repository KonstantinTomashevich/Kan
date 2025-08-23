#pragma once

#include <stddef.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/api_common/highlight.h>
#include <kan/error/critical.h>
#include <kan/universe/universe.h>

/// \file
/// \brief Provides convenience macros for universe mutators.
///
/// \par Motivation
/// \parblock
/// Query API is versatile, but it requires lots of things for proper setup. However, for the most cases, all of it
/// can be easily generated using Cushion preprocessor extensions, making writing of common case code for universe
/// mutators pretty easy and straightforward.
/// \endparblock
///
/// \par Macro prefixes
/// \parblock
/// Universe macros have set of prefixes that categorizes them into separate categories to make their usage patterns
/// more obvious:
///
/// - KAN_UM_* are generic macros for generic use cases like binding statement accumulators, generating signatures,
///   managing accesses and so on.
///
/// - KAN_UM_INTERNAL_* are macros for internal usage and should not be directly used.
///
/// - KAN_UMI_* are inline query macros -- macros that do not have their own scope and place their accesses and cursors
///   in the same scope that they're used.
///
/// - KAN_UMO_* are execute-once block query macros -- they're wrapper macros that are guaranteed to execute wrapped
///   block only once and they also guarantee that wrapped block is under if, so it is not a target for break/continue.
///   Also, block is not guaranteed to be executed, for example when event insert is declined as event is not used.
///
/// - KAN_UML_* are looped block query macros -- they iterate over all query results in a loop, so they're are always a
///   target for break/continue. If query has no results, wrapped block would never be executed.
/// \endparblock
///
/// \par State registration
/// \parblock
/// - KAN_UM_GENERATE_STATE_QUERIES is used to declare statement accumulator for mutator query fields.
///
/// - KAN_UM_BIND_STATE is used to declare for which mutator state queries are being generated and also uniform path
///   to this mutator instance.
///
/// - KAN_UM_BIND_STATE_FIELDLESS is used to declare path to mutator state while disabling query field generation.
///   Useful to functions for mixin substates like some transform helpers.
///
/// - KAN_UM_UNBIND_STATE is an optional helper for removing any mutator state binding.
/// \endparblock
///
/// \par Signatures
/// \parblock
/// - KAN_UM_MUTATOR_DEPLOY/KAN_UM_MUTATOR_EXECUTE/KAN_UM_MUTATOR_UNDEPLOY are used to declare function with appropriate
///   name and signature for given mutator name. API macro should still be prepended as it is not included.
///
/// - KAN_UM_MUTATOR_DEPLOY_SIGNATURE/KAN_UM_MUTATOR_EXECUTE_SIGNATURE/KAN_UM_MUTATOR_UNDEPLOY_SIGNATURE are used
///   to declare functions with arbitrary name, but appropriate signatures, which is useful for generated mutators.
///
/// - KAN_UM_SCHEDULER_DEPLOY/KAN_UM_SCHEDULER_EXECUTE/KAN_UM_SCHEDULER_UNDEPLOY and
///   KAN_UM_SCHEDULER_DEPLOY_SIGNATURE/KAN_UM_SCHEDULER_EXECUTE_SIGNATURE/KAN_UM_SCHEDULER_UNDEPLOY_SIGNATURE serve
///   the same purpose for schedulers.
/// \endparblock
///
/// \par Meta
/// \parblock
/// KAN_UM_MUTATOR_GROUP_META and KAN_UM_MUTATOR_GROUP_AUTO_NAME_META are used to generate
/// kan_universe_mutator_group_meta_t static variables. KAN_UM_ADD_MUTATOR_TO_FOLLOWING_GROUP
/// should be used to add mutators to that groups.
/// \endparblock
///
/// \par Access
/// \parblock
/// - KAN_UM_ACCESS_ESCAPE should be used to copy given access to given target field and mark it as escaped so it
///   won't be closed on scope exit.
///
/// - KAN_UM_ACCESS_DELETE should be used to delete record to which access points if it is supported by the access.
/// \endparblock
///
/// \par Queries
/// \parblock
/// Wide variety of macros is provided for more convenient work with queries:
///
/// - KAN_UMI_SINGLETON_READ and KAN_UMI_SINGLETON_WRITE are provided for singleton access.
///
/// - KAN_UMO_INDEXED_INSERT is provided for indexed insertion query.
///
/// - KAN_UML_(SEQUENCE|VALUE|SIGNAL|INTERVAL_ASCENDING|INTERVAL_DESCENDING)_(READ|UPDATE|DELETE|WRITE) are loop based
///   wrappers for listed types of queries with listed access patterns.
///
/// - There is also DETACH access for VALUE queries for the cases when we can guarantee that deletion happens in a way
///   that is compliant with KAN_WORKFLOW_RESOURCE_ACCESS_CLASS_POPULATION.
///
/// - There are KAN_UMI_VALUE_(READ|UPDATE|DELETE|DETACH|WRITE)_REQUIRED queries for the cases when we expect exactly
///   one result from the query, which is validated using assert.
///
/// - There are KAN_UMI_VALUE_(READ|UPDATE|DELETE|DETACH|WRITE)_OPTIONAL queries for the cases when we expect either
///   one result or no result from the query, which is validated using assert. When there is no result, query record
///   variable is set to NULL.
///
/// - KAN_UMO_EVENT_INSERT is provided for event insertion.
///
/// - KAN_UML_EVENT_FETCH is provided for fetching events of given type.
/// \endparblock

KAN_C_HEADER_BEGIN

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UM_GENERATE_STATE_QUERIES(STATE_NAME)                                                                  \
        /* Highlight-autocomplete replacement. */                                                                      \
        kan_memory_size_t STATE_NAME##_fake_placeholder_field;
#else
#    define KAN_UM_GENERATE_STATE_QUERIES(STATE_NAME) CUSHION_STATEMENT_ACCUMULATOR (universe_queries_##STATE_NAME)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UM_BIND_STATE(STATE_NAME, ...) /* No highlight-time replacement. */
#else
#    define KAN_UM_BIND_STATE(STATE_NAME, ...)                                                                         \
        CUSHION_STATEMENT_ACCUMULATOR_REF (universe_queries, universe_queries_##STATE_NAME)                            \
        CUSHION_SNIPPET (KAN_UM_STATE_PATH, (__VA_ARGS__))
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UM_BIND_STATE_FIELDLESS(STATE_NAME, ...) /* No highlight-time replacement. */
#else
#    define KAN_UM_BIND_STATE_FIELDLESS(STATE_NAME, ...)                                                               \
        CUSHION_STATEMENT_ACCUMULATOR_UNREF (universe_queries)                                                         \
        CUSHION_SNIPPET (KAN_UM_STATE_PATH, (__VA_ARGS__))
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UM_UNBIND_STATE /* No highlight-time replacement. */
#else
#    define KAN_UM_UNBIND_STATE                                                                                        \
        CUSHION_STATEMENT_ACCUMULATOR_UNREF (universe_queries)                                                         \
        CUSHION_SNIPPET (KAN_UM_STATE_PATH, (kan_up_state_path_not_initialized))
#endif

#define KAN_UM_MUTATOR_DEPLOY_SIGNATURE(FUNCTION_NAME, STATE_TYPE)                                                     \
    void FUNCTION_NAME (kan_universe_t universe, kan_universe_world_t world, kan_repository_t world_repository,        \
                        kan_workflow_graph_node_t workflow_node, struct STATE_TYPE *state)

#define KAN_UM_MUTATOR_DEPLOY(MUTATOR_NAME)                                                                            \
    KAN_UM_MUTATOR_DEPLOY_SIGNATURE (kan_universe_mutator_deploy_##MUTATOR_NAME, MUTATOR_NAME##_state_t)

#define KAN_UM_MUTATOR_EXECUTE_SIGNATURE(FUNCTION_NAME, STATE_TYPE)                                                    \
    void FUNCTION_NAME (kan_cpu_job_t job, struct STATE_TYPE *state)

#define KAN_UM_MUTATOR_EXECUTE(MUTATOR_NAME)                                                                           \
    KAN_UM_MUTATOR_EXECUTE_SIGNATURE (kan_universe_mutator_execute_##MUTATOR_NAME, MUTATOR_NAME##_state_t)

#define KAN_UM_MUTATOR_UNDEPLOY_SIGNATURE(FUNCTION_NAME, STATE_TYPE) void FUNCTION_NAME (struct STATE_TYPE *state)

#define KAN_UM_MUTATOR_UNDEPLOY(MUTATOR_NAME)                                                                          \
    KAN_UM_MUTATOR_UNDEPLOY_SIGNATURE (kan_universe_mutator_undeploy_##MUTATOR_NAME, MUTATOR_NAME##_state_t)

#define KAN_UM_ADD_MUTATOR_TO_FOLLOWING_GROUP(MUTATOR_NAME)                                                            \
    KAN_REFLECTION_FUNCTION_META (kan_universe_mutator_execute_##MUTATOR_NAME)

#define KAN_UM_MUTATOR_GROUP_META(GROUP_NAME_VARIABLE, GROUP_NAME_LITERAL)                                             \
    struct kan_universe_mutator_group_meta_t universe_mutator_group_meta_##GROUP_NAME_VARIABLE = {                     \
        .group_name = GROUP_NAME_LITERAL,                                                                              \
    }

#define KAN_UM_MUTATOR_GROUP_AUTO_NAME_META(GROUP_NAME)                                                                \
    struct kan_universe_mutator_group_meta_t universe_mutator_group_meta_##GROUP_NAME = {                              \
        .group_name = #GROUP_NAME,                                                                                     \
    }

#define KAN_UM_SCHEDULER_DEPLOY_SIGNATURE(FUNCTION_NAME, STATE_TYPE)                                                   \
    void FUNCTION_NAME (kan_universe_t universe, kan_universe_world_t world, kan_repository_t world_repository,        \
                        struct STATE_TYPE *state)

#define KAN_UM_SCHEDULER_DEPLOY(SCHEDULER_NAME)                                                                        \
    KAN_UM_SCHEDULER_DEPLOY_SIGNATURE (kan_universe_scheduler_deploy_##SCHEDULER_NAME,                                 \
                                       SCHEDULER_NAME##_scheduler_state_t)

#define KAN_UM_SCHEDULER_EXECUTE_SIGNATURE(FUNCTION_NAME, STATE_TYPE)                                                  \
    void FUNCTION_NAME (kan_universe_scheduler_interface_t interface, struct STATE_TYPE *state)

#define KAN_UM_SCHEDULER_EXECUTE(SCHEDULER_NAME)                                                                       \
    KAN_UM_SCHEDULER_EXECUTE_SIGNATURE (kan_universe_scheduler_execute_##SCHEDULER_NAME,                               \
                                        SCHEDULER_NAME##_scheduler_state_t)

#define KAN_UM_SCHEDULER_UNDEPLOY_SIGNATURE(FUNCTION_NAME, STATE_TYPE) void FUNCTION_NAME (struct STATE_TYPE *state)

#define KAN_UM_SCHEDULER_UNDEPLOY(SCHEDULER_NAME)                                                                      \
    KAN_UM_SCHEDULER_UNDEPLOY_SIGNATURE (kan_universe_scheduler_undeploy_##SCHEDULER_NAME,                             \
                                         SCHEDULER_NAME##_scheduler_state_t)

#define KAN_UM_ACCESS_ESCAPE(TARGET, NAME)                                                                             \
    TARGET = NAME##_access;                                                                                            \
    *(typeof_unqual (NAME) *) &NAME = NULL;

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UM_ACCESS_DELETE(NAME)                                                                                 \
        /* Highlight results in error with "no variable" if query highlight didn't declare this variable marking       \
         * delete as allowed for this query type. */                                                                   \
        delete_allowed_for_highlight_##NAME = true;                                                                    \
        *(typeof_unqual (NAME) *) &NAME = NULL
#else
#    define KAN_UM_ACCESS_DELETE(NAME)                                                                                 \
        KAN_SNIPPET_DELETE_ACCESS_##NAME;                                                                              \
        *(typeof_unqual (NAME) *) &NAME = NULL
#endif

#define KAN_UM_INTERNAL_STATE_FIELD(QUERY_TYPE, FIELD_NAME)                                                            \
    CUSHION_STATEMENT_ACCUMULATOR_PUSH (universe_queries, unique, optional) { struct QUERY_TYPE FIELD_NAME; }

#define KAN_UM_INTERNAL_ACCESS_DEFER(NAME, CLOSE_FUNCTION)                                                             \
    CUSHION_DEFER                                                                                                      \
    {                                                                                                                  \
        if (NAME)                                                                                                      \
        {                                                                                                              \
            CLOSE_FUNCTION (&NAME##_access);                                                                           \
        }                                                                                                              \
    }

#define KAN_UM_INTERNAL_SINGLETON(NAME, TYPE, ACCESS, QUALIFIER)                                                       \
    KAN_UM_INTERNAL_STATE_FIELD (kan_repository_singleton_##ACCESS##_query_t,                                          \
                                 ACCESS##__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE))                                    \
                                                                                                                       \
    struct kan_repository_singleton_##ACCESS##_access_t NAME##_access =                                                \
        kan_repository_singleton_##ACCESS##_query_execute (                                                            \
            &KAN_UM_STATE_PATH->ACCESS##__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE));                                    \
                                                                                                                       \
    QUALIFIER struct TYPE *const NAME = kan_repository_singleton_##ACCESS##_access_resolve (&NAME##_access);           \
    KAN_UM_INTERNAL_ACCESS_DEFER (NAME, kan_repository_singleton_##ACCESS##_access_close)

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UMI_SINGLETON_READ(NAME, TYPE)                                                                         \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        struct kan_repository_singleton_read_access_t NAME##_access = {0};
#else
#    define KAN_UMI_SINGLETON_READ(NAME, TYPE) KAN_UM_INTERNAL_SINGLETON (NAME, TYPE, read, const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UMI_SINGLETON_WRITE(NAME, TYPE)                                                                        \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        struct kan_repository_singleton_write_access_t NAME##_access = {0};
#else
#    define KAN_UMI_SINGLETON_WRITE(NAME, TYPE) KAN_UM_INTERNAL_SINGLETON (NAME, TYPE, write, )
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UMO_INDEXED_INSERT(NAME, TYPE)                                                                         \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;
#else
#    define KAN_UMO_INDEXED_INSERT(NAME, TYPE)                                                                         \
        {                                                                                                              \
            KAN_UM_INTERNAL_STATE_FIELD (kan_repository_indexed_insert_query_t,                                        \
                                         insert__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE))                              \
                                                                                                                       \
            struct kan_repository_indexed_insertion_package_t NAME##_package =                                         \
                kan_repository_indexed_insert_query_execute (                                                          \
                    &KAN_UM_STATE_PATH->insert__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE));                              \
                                                                                                                       \
            struct TYPE *NAME = kan_repository_indexed_insertion_package_get (&NAME##_package);                        \
            if (NAME)                                                                                                  \
            {                                                                                                          \
                CUSHION_DEFER { kan_repository_indexed_insertion_package_submit (&NAME##_package); }                   \
                __CUSHION_WRAPPED__                                                                                    \
            }                                                                                                          \
        }
#endif

#define KAN_UM_INTERNAL_SEQUENCE(NAME, TYPE, ACCESS, QUALIFIER)                                                        \
    {                                                                                                                  \
        KAN_UM_INTERNAL_STATE_FIELD (kan_repository_indexed_sequence_##ACCESS##_query_t,                               \
                                     ACCESS##_sequence__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE))                       \
                                                                                                                       \
        struct kan_repository_indexed_sequence_##ACCESS##_cursor_t NAME##_cursor =                                     \
            kan_repository_indexed_sequence_##ACCESS##_query_execute (                                                 \
                &KAN_UM_STATE_PATH->ACCESS##_sequence__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE));                       \
                                                                                                                       \
        CUSHION_DEFER { kan_repository_indexed_sequence_##ACCESS##_cursor_close (&NAME##_cursor); }                    \
        while (true)                                                                                                   \
        {                                                                                                              \
            struct kan_repository_indexed_sequence_##ACCESS##_access_t NAME##_access =                                 \
                kan_repository_indexed_sequence_##ACCESS##_cursor_next (&NAME##_cursor);                               \
            QUALIFIER struct TYPE *const NAME =                                                                        \
                kan_repository_indexed_sequence_##ACCESS##_access_resolve (&NAME##_access);                            \
                                                                                                                       \
            if (NAME)                                                                                                  \
            {                                                                                                          \
                KAN_UM_INTERNAL_ACCESS_DEFER (NAME, kan_repository_indexed_sequence_##ACCESS##_access_close)           \
                CUSHION_SNIPPET (KAN_SNIPPET_DELETE_ACCESS_##NAME,                                                     \
                                 kan_repository_indexed_sequence_##ACCESS##_access_delete (&NAME##_access))            \
                                                                                                                       \
                __CUSHION_WRAPPED__                                                                                    \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                break;                                                                                                 \
            }                                                                                                          \
        }                                                                                                              \
    }

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_SEQUENCE_READ(NAME, TYPE)                                                                          \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        struct kan_repository_indexed_sequence_read_access_t NAME##_access = {0};                                      \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_SEQUENCE_READ(NAME, TYPE) KAN_UM_INTERNAL_SEQUENCE (NAME, TYPE, read, const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_SEQUENCE_UPDATE(NAME, TYPE)                                                                        \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        struct kan_repository_indexed_sequence_update_access_t NAME##_access = {0};                                    \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_SEQUENCE_UPDATE(NAME, TYPE) KAN_UM_INTERNAL_SEQUENCE (NAME, TYPE, update, )
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_SEQUENCE_DELETE(NAME, TYPE)                                                                        \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        struct kan_repository_indexed_sequence_delete_access_t NAME##_access = {0};                                    \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_SEQUENCE_DELETE(NAME, TYPE) KAN_UM_INTERNAL_SEQUENCE (NAME, TYPE, delete, const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_SEQUENCE_WRITE(NAME, TYPE)                                                                         \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        struct kan_repository_indexed_sequence_write_access_t NAME##_access = {0};                                     \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_SEQUENCE_WRITE(NAME, TYPE) KAN_UM_INTERNAL_SEQUENCE (NAME, TYPE, write, )
#endif

#define KAN_UM_INTERNAL_VALUE(NAME, TYPE, FIELD, ARGUMENT_POINTER, ACCESS_TYPE, ACCESS_NAME, QUALIFIER)                \
    {                                                                                                                  \
        KAN_UM_INTERNAL_STATE_FIELD (kan_repository_indexed_value_##ACCESS_TYPE##_query_t,                             \
                                     ACCESS_NAME##_value__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE)##__##FIELD)          \
                                                                                                                       \
        struct kan_repository_indexed_value_##ACCESS_TYPE##_cursor_t NAME##_cursor =                                   \
            kan_repository_indexed_value_##ACCESS_TYPE##_query_execute (                                               \
                &KAN_UM_STATE_PATH->ACCESS_NAME##_value__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE)##__##FIELD,           \
                ARGUMENT_POINTER);                                                                                     \
                                                                                                                       \
        CUSHION_DEFER { kan_repository_indexed_value_##ACCESS_TYPE##_cursor_close (&NAME##_cursor); }                  \
        while (true)                                                                                                   \
        {                                                                                                              \
            struct kan_repository_indexed_value_##ACCESS_TYPE##_access_t NAME##_access =                               \
                kan_repository_indexed_value_##ACCESS_TYPE##_cursor_next (&NAME##_cursor);                             \
            QUALIFIER struct TYPE *const NAME =                                                                        \
                kan_repository_indexed_value_##ACCESS_TYPE##_access_resolve (&NAME##_access);                          \
                                                                                                                       \
            if (NAME)                                                                                                  \
            {                                                                                                          \
                KAN_UM_INTERNAL_ACCESS_DEFER (NAME, kan_repository_indexed_value_##ACCESS_TYPE##_access_close)         \
                CUSHION_SNIPPET (KAN_SNIPPET_DELETE_ACCESS_##NAME,                                                     \
                                 kan_repository_indexed_value_##ACCESS_TYPE##_access_delete (&NAME##_access))          \
                                                                                                                       \
                __CUSHION_WRAPPED__                                                                                    \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                break;                                                                                                 \
            }                                                                                                          \
        }                                                                                                              \
    }

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_VALUE_READ(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                                    \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_value_read_access_t NAME##_access = {0};                                         \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_pointer_for_highlight_##NAME = ARGUMENT_POINTER;                                          \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_VALUE_READ(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                                    \
        KAN_UM_INTERNAL_VALUE (NAME, TYPE, FIELD, ARGUMENT_POINTER, read, read, const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_VALUE_UPDATE(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                                  \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_value_update_access_t NAME##_access = {0};                                       \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_pointer_for_highlight_##NAME = ARGUMENT_POINTER;                                          \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_VALUE_UPDATE(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                                  \
        KAN_UM_INTERNAL_VALUE (NAME, TYPE, FIELD, ARGUMENT_POINTER, update, update, )
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_VALUE_DELETE(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                                  \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_value_delete_access_t NAME##_access = {0};                                       \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_pointer_for_highlight_##NAME = ARGUMENT_POINTER;                                          \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_VALUE_DELETE(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                                  \
        KAN_UM_INTERNAL_VALUE (NAME, TYPE, FIELD, ARGUMENT_POINTER, delete, delete, const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_VALUE_DETACH(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                                  \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_value_delete_access_t NAME##_access = {0};                                       \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_pointer_for_highlight_##NAME = ARGUMENT_POINTER;                                          \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_VALUE_DETACH(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                                  \
        KAN_UM_INTERNAL_VALUE (NAME, TYPE, FIELD, ARGUMENT_POINTER, delete, detach, const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_VALUE_WRITE(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                                   \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_value_write_access_t NAME##_access = {0};                                        \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_pointer_for_highlight_##NAME = ARGUMENT_POINTER;                                          \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_VALUE_WRITE(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                                   \
        KAN_UM_INTERNAL_VALUE (NAME, TYPE, FIELD, ARGUMENT_POINTER, write, write, )
#endif

#if defined(KAN_WITH_ASSERT)
#    define KAN_UM_INTERNAL_VALUE_UNIQUENESS_CHECK(NAME, ACCESS_TYPE)                                                  \
        struct kan_repository_indexed_value_##ACCESS_TYPE##_access_t uniqueness_check_##NAME##_access =                \
            kan_repository_indexed_value_##ACCESS_TYPE##_cursor_next (&NAME##_cursor);                                 \
        KAN_ASSERT (!kan_repository_indexed_value_##ACCESS_TYPE##_access_resolve (&uniqueness_check_##NAME##_access))
#else
#    define KAN_UM_INTERNAL_VALUE_UNIQUENESS_CHECK(NAME, ACCESS_TYPE) /* No assert, check disabled. */
#endif

#define KAN_UM_INTERNAL_VALUE_REQUIRED(NAME, TYPE, FIELD, ARGUMENT_POINTER, ACCESS_TYPE, ACCESS_NAME, QUALIFIER)       \
                                                                                                                       \
    KAN_UM_INTERNAL_STATE_FIELD (kan_repository_indexed_value_##ACCESS_TYPE##_query_t,                                 \
                                 ACCESS_NAME##_value__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE)##__##FIELD)              \
                                                                                                                       \
    struct kan_repository_indexed_value_##ACCESS_TYPE##_cursor_t NAME##_cursor =                                       \
        kan_repository_indexed_value_##ACCESS_TYPE##_query_execute (                                                   \
            &KAN_UM_STATE_PATH->ACCESS_NAME##_value__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE)##__##FIELD,               \
            ARGUMENT_POINTER);                                                                                         \
                                                                                                                       \
    struct kan_repository_indexed_value_##ACCESS_TYPE##_access_t NAME##_access =                                       \
        kan_repository_indexed_value_##ACCESS_TYPE##_cursor_next (&NAME##_cursor);                                     \
                                                                                                                       \
    QUALIFIER struct TYPE *const NAME = kan_repository_indexed_value_##ACCESS_TYPE##_access_resolve (&NAME##_access);  \
    KAN_ASSERT (NAME); /* Require macro expects that there is always one value. */                                     \
    KAN_UM_INTERNAL_ACCESS_DEFER (NAME, kan_repository_indexed_value_##ACCESS_TYPE##_access_close)                     \
                                                                                                                       \
    CUSHION_SNIPPET (KAN_SNIPPET_DELETE_ACCESS_##NAME,                                                                 \
                     kan_repository_indexed_value_##ACCESS_TYPE##_access_delete (&NAME##_access))                      \
                                                                                                                       \
    KAN_UM_INTERNAL_VALUE_UNIQUENESS_CHECK (NAME, ACCESS_TYPE)                                                         \
    kan_repository_indexed_value_##ACCESS_TYPE##_cursor_close (&NAME##_cursor);

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UMI_VALUE_READ_REQUIRED(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                           \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_value_read_access_t NAME##_access = {0};                                         \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_pointer_for_highlight_##NAME = ARGUMENT_POINTER;
#else
#    define KAN_UMI_VALUE_READ_REQUIRED(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                           \
        KAN_UM_INTERNAL_VALUE_REQUIRED (NAME, TYPE, FIELD, ARGUMENT_POINTER, read, read, const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UMI_VALUE_UPDATE_REQUIRED(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                         \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_value_update_access_t NAME##_access = {0};                                       \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_pointer_for_highlight_##NAME = ARGUMENT_POINTER;
#else
#    define KAN_UMI_VALUE_UPDATE_REQUIRED(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                         \
        KAN_UM_INTERNAL_VALUE_REQUIRED (NAME, TYPE, FIELD, ARGUMENT_POINTER, update, update, )
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UMI_VALUE_DELETE_REQUIRED(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                         \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_value_delete_access_t NAME##_access = {0};                                       \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_pointer_for_highlight_##NAME = ARGUMENT_POINTER;
#else
#    define KAN_UMI_VALUE_DELETE_REQUIRED(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                         \
        KAN_UM_INTERNAL_VALUE_REQUIRED (NAME, TYPE, FIELD, ARGUMENT_POINTER, delete, delete, const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UMI_VALUE_DETACH_REQUIRED(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                         \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_value_delete_access_t NAME##_access = {0};                                       \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_pointer_for_highlight_##NAME = ARGUMENT_POINTER;
#else
#    define KAN_UMI_VALUE_DETACH_REQUIRED(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                         \
        KAN_UM_INTERNAL_VALUE_REQUIRED (NAME, TYPE, FIELD, ARGUMENT_POINTER, delete, detach, const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UMI_VALUE_WRITE_REQUIRED(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                          \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_value_write_access_t NAME##_access = {0};                                        \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_pointer_for_highlight_##NAME = ARGUMENT_POINTER;
#else
#    define KAN_UMI_VALUE_WRITE_REQUIRED(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                          \
        KAN_UM_INTERNAL_VALUE_REQUIRED (NAME, TYPE, FIELD, ARGUMENT_POINTER, write, write, )
#endif

#define KAN_UM_INTERNAL_VALUE_OPTIONAL(NAME, TYPE, FIELD, ARGUMENT_POINTER, ACCESS_TYPE, ACCESS_NAME, QUALIFIER)       \
                                                                                                                       \
    KAN_UM_INTERNAL_STATE_FIELD (kan_repository_indexed_value_##ACCESS_TYPE##_query_t,                                 \
                                 ACCESS_NAME##_value__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE)##__##FIELD)              \
                                                                                                                       \
    struct kan_repository_indexed_value_##ACCESS_TYPE##_cursor_t NAME##_cursor =                                       \
        kan_repository_indexed_value_##ACCESS_TYPE##_query_execute (                                                   \
            &KAN_UM_STATE_PATH->ACCESS_NAME##_value__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE)##__##FIELD,               \
            ARGUMENT_POINTER);                                                                                         \
                                                                                                                       \
    struct kan_repository_indexed_value_##ACCESS_TYPE##_access_t NAME##_access =                                       \
        kan_repository_indexed_value_##ACCESS_TYPE##_cursor_next (&NAME##_cursor);                                     \
                                                                                                                       \
    QUALIFIER struct TYPE *const NAME = kan_repository_indexed_value_##ACCESS_TYPE##_access_resolve (&NAME##_access);  \
    KAN_UM_INTERNAL_ACCESS_DEFER (NAME, kan_repository_indexed_value_##ACCESS_TYPE##_access_close)                     \
                                                                                                                       \
    CUSHION_SNIPPET (KAN_SNIPPET_DELETE_ACCESS_##NAME,                                                                 \
                     kan_repository_indexed_value_##ACCESS_TYPE##_access_delete (&NAME##_access))                      \
                                                                                                                       \
    if (NAME)                                                                                                          \
    {                                                                                                                  \
        /* Optional macro expects that there cannot be several values. */                                              \
        KAN_UM_INTERNAL_VALUE_UNIQUENESS_CHECK (NAME, ACCESS_TYPE)                                                     \
    }                                                                                                                  \
                                                                                                                       \
    kan_repository_indexed_value_##ACCESS_TYPE##_cursor_close (&NAME##_cursor);

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UMI_VALUE_READ_OPTIONAL(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                           \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_value_read_access_t NAME##_access = {0};                                         \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_pointer_for_highlight_##NAME = ARGUMENT_POINTER;
#else
#    define KAN_UMI_VALUE_READ_OPTIONAL(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                           \
        KAN_UM_INTERNAL_VALUE_OPTIONAL (NAME, TYPE, FIELD, ARGUMENT_POINTER, read, read, const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UMI_VALUE_UPDATE_OPTIONAL(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                         \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_value_update_access_t NAME##_access = {0};                                       \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_pointer_for_highlight_##NAME = ARGUMENT_POINTER;
#else
#    define KAN_UMI_VALUE_UPDATE_OPTIONAL(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                         \
        KAN_UM_INTERNAL_VALUE_OPTIONAL (NAME, TYPE, FIELD, ARGUMENT_POINTER, update, update, )
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UMI_VALUE_DELETE_OPTIONAL(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                         \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_value_delete_access_t NAME##_access = {0};                                       \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_pointer_for_highlight_##NAME = ARGUMENT_POINTER;
#else
#    define KAN_UMI_VALUE_DELETE_OPTIONAL(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                         \
        KAN_UM_INTERNAL_VALUE_OPTIONAL (NAME, TYPE, FIELD, ARGUMENT_POINTER, delete, delete, const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UMI_VALUE_DETACH_OPTIONAL(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                         \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_value_delete_access_t NAME##_access = {0};                                       \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_pointer_for_highlight_##NAME = ARGUMENT_POINTER;
#else
#    define KAN_UMI_VALUE_DETACH_OPTIONAL(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                         \
        KAN_UM_INTERNAL_VALUE_OPTIONAL (NAME, TYPE, FIELD, ARGUMENT_POINTER, delete, detach, const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UMI_VALUE_WRITE_OPTIONAL(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                          \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_value_write_access_t NAME##_access = {0};                                        \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_pointer_for_highlight_##NAME = ARGUMENT_POINTER;
#else
#    define KAN_UMI_VALUE_WRITE_OPTIONAL(NAME, TYPE, FIELD, ARGUMENT_POINTER)                                          \
        KAN_UM_INTERNAL_VALUE_OPTIONAL (NAME, TYPE, FIELD, ARGUMENT_POINTER, write, write, )
#endif

#define KAN_UM_INTERNAL_SIGNAL(NAME, TYPE, FIELD, LITERAL_VALUE, ACCESS, QUALIFIER)                                    \
    {                                                                                                                  \
        KAN_UM_INTERNAL_STATE_FIELD (kan_repository_indexed_signal_##ACCESS##_query_t,                                 \
                                     ACCESS##_signal__##__CUSHION_EVALUATED_ARGUMENT__ (                               \
                                         TYPE)##__##FIELD##__##__CUSHION_EVALUATED_ARGUMENT__ (LITERAL_VALUE))         \
                                                                                                                       \
        struct kan_repository_indexed_signal_##ACCESS##_cursor_t NAME##_cursor =                                       \
            kan_repository_indexed_signal_##ACCESS##_query_execute (                                                   \
                &KAN_UM_STATE_PATH->ACCESS##_signal__##__CUSHION_EVALUATED_ARGUMENT__ (                                \
                    TYPE)##__##FIELD##__##__CUSHION_EVALUATED_ARGUMENT__ (LITERAL_VALUE));                             \
                                                                                                                       \
        CUSHION_DEFER { kan_repository_indexed_signal_##ACCESS##_cursor_close (&NAME##_cursor); }                      \
        while (true)                                                                                                   \
        {                                                                                                              \
            struct kan_repository_indexed_signal_##ACCESS##_access_t NAME##_access =                                   \
                kan_repository_indexed_signal_##ACCESS##_cursor_next (&NAME##_cursor);                                 \
            QUALIFIER struct TYPE *const NAME =                                                                        \
                kan_repository_indexed_signal_##ACCESS##_access_resolve (&NAME##_access);                              \
                                                                                                                       \
            if (NAME)                                                                                                  \
            {                                                                                                          \
                KAN_UM_INTERNAL_ACCESS_DEFER (NAME, kan_repository_indexed_signal_##ACCESS##_access_close)             \
                CUSHION_SNIPPET (KAN_SNIPPET_DELETE_ACCESS_##NAME,                                                     \
                                 kan_repository_indexed_signal_##ACCESS##_access_delete (&NAME##_access))              \
                                                                                                                       \
                __CUSHION_WRAPPED__                                                                                    \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                break;                                                                                                 \
            }                                                                                                          \
        }                                                                                                              \
    }

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_SIGNAL_READ(NAME, TYPE, FIELD, LITERAL_SIGNAL)                                                     \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_signal_read_access_t NAME##_access = {0};                                        \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_SIGNAL_READ(NAME, TYPE, FIELD, LITERAL_SIGNAL)                                                     \
        KAN_UM_INTERNAL_SIGNAL (NAME, TYPE, FIELD, LITERAL_SIGNAL, read, const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_SIGNAL_UPDATE(NAME, TYPE, FIELD, LITERAL_SIGNAL)                                                   \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_signal_update_access_t NAME##_access = {0};                                      \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_SIGNAL_UPDATE(NAME, TYPE, FIELD, LITERAL_SIGNAL)                                                   \
        KAN_UM_INTERNAL_SIGNAL (NAME, TYPE, FIELD, LITERAL_SIGNAL, update, )
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_SIGNAL_DELETE(NAME, TYPE, FIELD, LITERAL_SIGNAL)                                                   \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_signal_delete_access_t NAME##_access = {0};                                      \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_SIGNAL_DELETE(NAME, TYPE, FIELD, LITERAL_SIGNAL)                                                   \
        KAN_UM_INTERNAL_SIGNAL (NAME, TYPE, FIELD, LITERAL_SIGNAL, delete, const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_SIGNAL_WRITE(NAME, TYPE, FIELD, LITERAL_SIGNAL)                                                    \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_signal_write_access_t NAME##_access = {0};                                       \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_SIGNAL_WRITE(NAME, TYPE, FIELD, LITERAL_SIGNAL)                                                    \
        KAN_UM_INTERNAL_SIGNAL (NAME, TYPE, FIELD, LITERAL_SIGNAL, write, )
#endif

#define KAN_UM_INTERNAL_INTERVAL(NAME, TYPE, FIELD, MIN_POINTER, MAX_POINTER, ACCESS, DIRECTION, QUALIFIER)            \
    {                                                                                                                  \
        KAN_UM_INTERNAL_STATE_FIELD (kan_repository_indexed_interval_##ACCESS##_query_t,                               \
                                     ACCESS##_interval__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE)##__##FIELD)            \
                                                                                                                       \
        struct kan_repository_indexed_interval_##DIRECTION##_##ACCESS##_cursor_t NAME##_cursor =                       \
            kan_repository_indexed_interval_##ACCESS##_query_execute_##DIRECTION (                                     \
                &KAN_UM_STATE_PATH->ACCESS##_interval__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE)##__##FIELD,             \
                MIN_POINTER, MAX_POINTER);                                                                             \
                                                                                                                       \
        CUSHION_DEFER { kan_repository_indexed_interval_##DIRECTION##_##ACCESS##_cursor_close (&NAME##_cursor); }      \
        while (true)                                                                                                   \
        {                                                                                                              \
            struct kan_repository_indexed_interval_##ACCESS##_access_t NAME##_access =                                 \
                kan_repository_indexed_interval_##DIRECTION##_##ACCESS##_cursor_next (&NAME##_cursor);                 \
            QUALIFIER struct TYPE *const NAME =                                                                        \
                kan_repository_indexed_interval_##ACCESS##_access_resolve (&NAME##_access);                            \
                                                                                                                       \
            if (NAME)                                                                                                  \
            {                                                                                                          \
                KAN_UM_INTERNAL_ACCESS_DEFER (NAME, kan_repository_indexed_interval_##ACCESS##_access_close)           \
                CUSHION_SNIPPET (KAN_SNIPPET_DELETE_ACCESS_##NAME,                                                     \
                                 kan_repository_indexed_interval_##ACCESS##_access_delete (&NAME##_access))            \
                                                                                                                       \
                __CUSHION_WRAPPED__                                                                                    \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                break;                                                                                                 \
            }                                                                                                          \
        }                                                                                                              \
    }

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_INTERVAL_ASCENDING_READ(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)             \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_interval_read_access_t NAME##_access = {0};                                      \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_min_pointer_for_highlight_##NAME = ARGUMENT_MIN_POINTER;                                  \
        const void *argument_max_pointer_for_highlight_##NAME = ARGUMENT_MAX_POINTER;                                  \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_INTERVAL_ASCENDING_READ(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)             \
        KAN_UM_INTERNAL_INTERVAL (NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER, read, ascending, const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_INTERVAL_ASCENDING_UPDATE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)           \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_interval_update_access_t NAME##_access = {0};                                    \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_min_pointer_for_highlight_##NAME = ARGUMENT_MIN_POINTER;                                  \
        const void *argument_max_pointer_for_highlight_##NAME = ARGUMENT_MAX_POINTER;                                  \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_INTERVAL_ASCENDING_UPDATE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)           \
        KAN_UM_INTERNAL_INTERVAL (NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER, update, ascending, )
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_INTERVAL_ASCENDING_DELETE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)           \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_interval_delete_access_t NAME##_access = {0};                                    \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_min_pointer_for_highlight_##NAME = ARGUMENT_MIN_POINTER;                                  \
        const void *argument_max_pointer_for_highlight_##NAME = ARGUMENT_MAX_POINTER;                                  \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_INTERVAL_ASCENDING_DELETE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)           \
        KAN_UM_INTERNAL_INTERVAL (NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER, delete, ascending,    \
                                  const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_INTERVAL_ASCENDING_WRITE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)            \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_interval_write_access_t NAME##_access = {0};                                     \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_min_pointer_for_highlight_##NAME = ARGUMENT_MIN_POINTER;                                  \
        const void *argument_max_pointer_for_highlight_##NAME = ARGUMENT_MAX_POINTER;                                  \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_INTERVAL_ASCENDING_WRITE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)            \
        KAN_UM_INTERNAL_INTERVAL (NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER, write, ascending, )
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_INTERVAL_DESCENDING_READ(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)            \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_interval_read_access_t NAME##_access = {0};                                      \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_min_pointer_for_highlight_##NAME = ARGUMENT_MIN_POINTER;                                  \
        const void *argument_max_pointer_for_highlight_##NAME = ARGUMENT_MAX_POINTER;                                  \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_INTERVAL_DESCENDING_READ(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)            \
        KAN_UM_INTERNAL_INTERVAL (NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER, read, descending,     \
                                  const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_INTERVAL_DESCENDING_UPDATE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)          \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_interval_update_access_t NAME##_access = {0};                                    \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_min_pointer_for_highlight_##NAME = ARGUMENT_MIN_POINTER;                                  \
        const void *argument_max_pointer_for_highlight_##NAME = ARGUMENT_MAX_POINTER;                                  \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_INTERVAL_DESCENDING_UPDATE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)          \
        KAN_UM_INTERNAL_INTERVAL (NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER, update, descending, )
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_INTERVAL_DESCENDING_DELETE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)          \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_interval_delete_access_t NAME##_access = {0};                                    \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_min_pointer_for_highlight_##NAME = ARGUMENT_MIN_POINTER;                                  \
        const void *argument_max_pointer_for_highlight_##NAME = ARGUMENT_MAX_POINTER;                                  \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_INTERVAL_DESCENDING_DELETE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)          \
        KAN_UM_INTERNAL_INTERVAL (NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER, delete, descending,   \
                                  const)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_INTERVAL_DESCENDING_WRITE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)           \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;                                                                                      \
        KAN_HIGHLIGHT_STRUCT_FIELD (TYPE, FIELD)                                                                       \
        struct kan_repository_indexed_interval_write_access_t NAME##_access = {0};                                     \
        bool delete_allowed_for_highlight_##NAME = false;                                                              \
        /* Do this manipulation so it always looks used for the IDE. */                                                \
        delete_allowed_for_highlight_##NAME = delete_allowed_for_highlight_##NAME + 1u;                                \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_min_pointer_for_highlight_##NAME = ARGUMENT_MIN_POINTER;                                  \
        const void *argument_max_pointer_for_highlight_##NAME = ARGUMENT_MAX_POINTER;                                  \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_INTERVAL_DESCENDING_WRITE(NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER)           \
        KAN_UM_INTERNAL_INTERVAL (NAME, TYPE, FIELD, ARGUMENT_MIN_POINTER, ARGUMENT_MAX_POINTER, write, descending, )
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UMO_EVENT_INSERT(NAME, TYPE)                                                                           \
        /* Highlight-autocomplete replacement. */                                                                      \
        struct TYPE *NAME = NULL;
#else
#    define KAN_UMO_EVENT_INSERT(NAME, TYPE)                                                                           \
        {                                                                                                              \
            KAN_UM_INTERNAL_STATE_FIELD (kan_repository_event_insert_query_t,                                          \
                                         insert__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE))                              \
                                                                                                                       \
            struct kan_repository_event_insertion_package_t NAME##_package =                                           \
                kan_repository_event_insert_query_execute (                                                            \
                    &KAN_UM_STATE_PATH->insert__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE));                              \
                                                                                                                       \
            struct TYPE *const NAME = kan_repository_event_insertion_package_get (&NAME##_package);                    \
            if (NAME)                                                                                                  \
            {                                                                                                          \
                CUSHION_DEFER { kan_repository_event_insertion_package_submit (&NAME##_package); }                     \
                __CUSHION_WRAPPED__                                                                                    \
            }                                                                                                          \
        }
#endif

#define KAN_UM_INTERNAL_EVENT_FETCH(NAME, TYPE, VARIABLE_TYPE)                                                         \
    {                                                                                                                  \
        KAN_UM_INTERNAL_STATE_FIELD (kan_repository_event_fetch_query_t,                                               \
                                     fetch__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE))                                   \
                                                                                                                       \
        while (true)                                                                                                   \
        {                                                                                                              \
            struct kan_repository_event_read_access_t NAME##_access = kan_repository_event_fetch_query_next (          \
                &KAN_UM_STATE_PATH->fetch__##__CUSHION_EVALUATED_ARGUMENT__ (TYPE));                                   \
            const struct VARIABLE_TYPE *const NAME = kan_repository_event_read_access_resolve (&NAME##_access);        \
                                                                                                                       \
            if (NAME)                                                                                                  \
            {                                                                                                          \
                KAN_UM_INTERNAL_ACCESS_DEFER (NAME, kan_repository_event_read_access_close)                            \
                                                                                                                       \
                __CUSHION_WRAPPED__                                                                                    \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                break;                                                                                                 \
            }                                                                                                          \
        }                                                                                                              \
    }

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_EVENT_FETCH(NAME, TYPE)                                                                            \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct TYPE *NAME = NULL;                                                                                \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_EVENT_FETCH(NAME, TYPE) KAN_UM_INTERNAL_EVENT_FETCH (NAME, TYPE, TYPE)
#endif

KAN_C_HEADER_END
