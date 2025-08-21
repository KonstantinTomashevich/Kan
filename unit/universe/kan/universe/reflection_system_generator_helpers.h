#pragma once

#include <qsort.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/mute_warnings.h>
#include <kan/reflection/registry.h>

/// \file
/// \brief Provides utility macros for reflection generators that generate universe mutators.

KAN_C_HEADER_BEGIN

/// \brief Base skeleton for iterate function of reflection generator that only observes structs with specific meta.
/// \details This macro creates skeleton with appropriate statement accumulators using SCANNER_NAME for naming the
///          accumulator. Actual logic should be added using KAN_UNIVERSE_REFLECTION_GENERATOR_ON_STRUCT_META_SCANNED.
#define KAN_UNIVERSE_REFLECTION_GENERATOR_STRUCT_META_SCANNER_CORE(SCANNER_NAME)                                       \
    if (iteration_index == instance->boostrap_iteration)                                                               \
    {                                                                                                                  \
        kan_reflection_registry_struct_iterator_t struct_iterator =                                                    \
            kan_reflection_registry_struct_iterator_create (registry);                                                 \
        const struct kan_reflection_struct_t *type;                                                                    \
                                                                                                                       \
        while ((type = kan_reflection_registry_struct_iterator_get (struct_iterator)))                                 \
        {                                                                                                              \
            CUSHION_STATEMENT_ACCUMULATOR (universe_reflection_generator_##SCANNER_NAME##_on_struct_bootstrap)         \
            CUSHION_STATEMENT_ACCUMULATOR_REF (universe_reflection_generator_on_struct_bootstrap,                      \
                                               universe_reflection_generator_##SCANNER_NAME##_on_struct_bootstrap)     \
                                                                                                                       \
            struct_iterator = kan_reflection_registry_struct_iterator_next (struct_iterator);                          \
        }                                                                                                              \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        kan_interned_string_t type_name;                                                                               \
        while ((type_name = kan_reflection_system_generation_iterator_next_added_struct (iterator)))                   \
        {                                                                                                              \
            const struct kan_reflection_struct_t *type = kan_reflection_registry_query_struct (registry, type_name);   \
            if (type)                                                                                                  \
            {                                                                                                          \
                CUSHION_STATEMENT_ACCUMULATOR (universe_reflection_generator_##SCANNER_NAME##_on_struct_new)           \
                CUSHION_STATEMENT_ACCUMULATOR_REF (universe_reflection_generator_on_struct_new,                        \
                                                   universe_reflection_generator_##SCANNER_NAME##_on_struct_new)       \
            }                                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        struct kan_reflection_system_added_struct_meta_t added_meta;                                                   \
        while ((added_meta = kan_reflection_system_generation_iterator_next_added_struct_meta (iterator)).struct_name) \
        {                                                                                                              \
            const struct kan_reflection_struct_t *type =                                                               \
                kan_reflection_registry_query_struct (registry, added_meta.struct_name);                               \
                                                                                                                       \
            if (type)                                                                                                  \
            {                                                                                                          \
                CUSHION_STATEMENT_ACCUMULATOR (universe_reflection_generator_##SCANNER_NAME##_on_struct_meta_new)      \
                CUSHION_STATEMENT_ACCUMULATOR_REF (universe_reflection_generator_on_struct_meta_new,                   \
                                                   universe_reflection_generator_##SCANNER_NAME##_on_struct_meta_new)  \
            }                                                                                                          \
        }                                                                                                              \
    }

/// \def KAN_UNIVERSE_REFLECTION_GENERATOR_ON_STRUCT_META_SCANNED
/// \brief Adds wrapped block to reflection generator struct with meta observation accumulators.
/// \param META_TYPE Name of the meta struct type without `struct` prefix.
/// \param META_TYPE_NAME_PATH Path to interned string with META_TYPE name for equality checks.

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UNIVERSE_REFLECTION_GENERATOR_ON_STRUCT_META_SCANNED(META_TYPE, META_TYPE_NAME_PATH)                   \
        const struct kan_reflection_struct_t *type = NULL;                                                             \
        const struct META_TYPE *meta = NULL;
#else
#    define KAN_UNIVERSE_REFLECTION_GENERATOR_ON_STRUCT_META_SCANNED(META_TYPE, META_TYPE_NAME_PATH)                   \
        CUSHION_STATEMENT_ACCUMULATOR_PUSH (universe_reflection_generator_on_struct_bootstrap)                         \
        {                                                                                                              \
            {                                                                                                          \
                struct kan_reflection_struct_meta_iterator_t meta_iterator =                                           \
                    kan_reflection_registry_query_struct_meta (registry, type->name, META_TYPE_NAME_PATH);             \
                const struct META_TYPE *meta = kan_reflection_struct_meta_iterator_get (&meta_iterator);               \
                                                                                                                       \
                if (meta)                                                                                              \
                {                                                                                                      \
                    __CUSHION_WRAPPED__                                                                                \
                }                                                                                                      \
            }                                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        CUSHION_STATEMENT_ACCUMULATOR_PUSH (universe_reflection_generator_on_struct_new)                               \
        {                                                                                                              \
            {                                                                                                          \
                struct kan_reflection_struct_meta_iterator_t meta_iterator =                                           \
                    kan_reflection_registry_query_struct_meta (registry, type->name, META_TYPE_NAME_PATH);             \
                const struct META_TYPE *meta = kan_reflection_struct_meta_iterator_get (&meta_iterator);               \
                                                                                                                       \
                if (meta)                                                                                              \
                {                                                                                                      \
                    __CUSHION_WRAPPED__                                                                                \
                }                                                                                                      \
            }                                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        CUSHION_STATEMENT_ACCUMULATOR_PUSH (universe_reflection_generator_on_struct_meta_new)                          \
        {                                                                                                              \
            if (added_meta.meta_type_name == META_TYPE_NAME_PATH)                                                      \
            {                                                                                                          \
                struct kan_reflection_struct_meta_iterator_t meta_iterator =                                           \
                    kan_reflection_registry_query_struct_meta (registry, type->name, META_TYPE_NAME_PATH);             \
                const struct META_TYPE *meta = kan_reflection_struct_meta_iterator_get (&meta_iterator);               \
                                                                                                                       \
                if (meta)                                                                                              \
                {                                                                                                      \
                    __CUSHION_WRAPPED__                                                                                \
                }                                                                                                      \
            }                                                                                                          \
        }
#endif

/// \brief Macro for extracting arguments in deploy function used for reflection generation purposes.
#define KAN_UNIVERSE_EXTRACT_DEPLOY_ARGUMENTS(NAME, ADDRESS, STATE_TYPE)                                               \
    struct                                                                                                             \
    {                                                                                                                  \
        kan_universe_t universe;                                                                                       \
        kan_universe_world_t world;                                                                                    \
        kan_repository_t world_repository;                                                                             \
        kan_workflow_graph_node_t workflow_node;                                                                       \
        STATE_TYPE *state;                                                                                             \
    } *NAME = ADDRESS

/// \brief Macro for extracting arguments in execute function used for reflection generation purposes.
#define KAN_UNIVERSE_EXTRACT_EXECUTE_ARGUMENTS(NAME, ADDRESS, STATE_TYPE)                                              \
    struct                                                                                                             \
    {                                                                                                                  \
        kan_cpu_job_t job;                                                                                             \
        STATE_TYPE *state;                                                                                             \
    } *NAME = ADDRESS

/// \brief Macro for extracting arguments in undeploy function used for reflection generation purposes.
#define KAN_UNIVERSE_EXTRACT_UNDEPLOY_ARGUMENTS(NAME, ADDRESS, STATE_TYPE)                                             \
    struct                                                                                                             \
    {                                                                                                                  \
        STATE_TYPE *state;                                                                                             \
    } *NAME = ADDRESS

/// \brief Helper for reflection generators that unwraps list of nodes into a temporary array.
/// \details Array is automatically deallocated using CUSHION_DEFER.
#define KAN_UNIVERSE_REFLECTION_GENERATOR_NODE_LIST_TO_TEMPORARY_ARRAY(NODE_TYPE, ARRAY_NAME, FIRST_NODE, COUNT,       \
                                                                       ALLOCATION_GROUP)                               \
    NODE_TYPE **ARRAY_NAME = kan_allocate_general (ALLOCATION_GROUP, sizeof (void *) * COUNT, alignof (void *));       \
    CUSHION_DEFER { kan_free_general (ALLOCATION_GROUP, ARRAY_NAME, sizeof (void *) * COUNT); }                        \
                                                                                                                       \
    NODE_TYPE *node = FIRST_NODE;                                                                                      \
    kan_loop_size_t output_index = 0u;                                                                                 \
                                                                                                                       \
    while (node)                                                                                                       \
    {                                                                                                                  \
        ARRAY_NAME[output_index] = node;                                                                               \
        ++output_index;                                                                                                \
        node = node->next;                                                                                             \
    }

/// \brief Helper for reflection generators that reorders nodes using given array.
#define KAN_UNIVERSE_REFLECTION_GENERATOR_NODE_REORDER_FROM_ARRAY(ARRAY_NAME, COUNT)                                   \
    for (kan_loop_size_t node_index = 0u; node_index < COUNT; ++node_index)                                            \
    {                                                                                                                  \
        if (node_index + 1u < COUNT)                                                                                   \
        {                                                                                                              \
            ARRAY_NAME[node_index]->next = ARRAY_NAME[node_index + 1u];                                                \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            ARRAY_NAME[node_index]->next = NULL;                                                                       \
        }                                                                                                              \
    }

/// \brief Helper for reflection generators that provides common way of filling generated mutator struct type.
#define KAN_UNIVERSE_REFLECTION_GENERATOR_MUTATOR_TYPE(OUTPUT, USER_DATA, MUTATOR_TYPE_NAME, BASE_TYPE_NAME,           \
                                                       GENERATED_NODE_TYPE_NAME, GENERATED_NODE_COUNT, INIT, SHUTDOWN, \
                                                       ALLOCATION_GROUP)                                               \
    (OUTPUT).name = kan_string_intern (#MUTATOR_TYPE_NAME);                                                            \
    (OUTPUT).alignment = alignof (struct BASE_TYPE_NAME);                                                              \
    (OUTPUT).size = sizeof (struct BASE_TYPE_NAME) + sizeof (struct GENERATED_NODE_TYPE_NAME) * GENERATED_NODE_COUNT;  \
                                                                                                                       \
    (OUTPUT).functor_user_data = (kan_functor_user_data_t) USER_DATA;                                                  \
    (OUTPUT).init = INIT;                                                                                              \
    (OUTPUT).shutdown = SHUTDOWN;                                                                                      \
                                                                                                                       \
    (OUTPUT).fields_count = 2u;                                                                                        \
    (OUTPUT).fields = kan_allocate_general (ALLOCATION_GROUP, sizeof (struct kan_reflection_field_t) * 2u,             \
                                            alignof (struct kan_reflection_field_t));                                  \
                                                                                                                       \
    (OUTPUT).fields[0u].name = kan_string_intern ("base_mutator_state");                                               \
    (OUTPUT).fields[0u].offset = 0u;                                                                                   \
    (OUTPUT).fields[0u].size = sizeof (struct BASE_TYPE_NAME);                                                         \
    (OUTPUT).fields[0u].archetype = KAN_REFLECTION_ARCHETYPE_STRUCT;                                                   \
    (OUTPUT).fields[0u].archetype_struct.type_name = kan_string_intern (#BASE_TYPE_NAME);                              \
    (OUTPUT).fields[0u].visibility_condition_field = NULL;                                                             \
    (OUTPUT).fields[0u].visibility_condition_values_count = 0u;                                                        \
    (OUTPUT).fields[0u].visibility_condition_values = NULL;                                                            \
                                                                                                                       \
    (OUTPUT).fields[1u].name = kan_string_intern ("container_types");                                                  \
    (OUTPUT).fields[1u].offset = sizeof (struct BASE_TYPE_NAME);                                                       \
    (OUTPUT).fields[1u].size = sizeof (struct GENERATED_NODE_TYPE_NAME) * GENERATED_NODE_COUNT;                        \
    (OUTPUT).fields[1u].archetype = KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY;                                             \
    (OUTPUT).fields[1u].archetype_inline_array.item_count = GENERATED_NODE_COUNT;                                      \
    (OUTPUT).fields[1u].archetype_inline_array.item_size = sizeof (struct GENERATED_NODE_TYPE_NAME);                   \
    (OUTPUT).fields[1u].archetype_inline_array.size_field = NULL;                                                      \
    (OUTPUT).fields[1u].archetype_inline_array.item_archetype = KAN_REFLECTION_ARCHETYPE_STRUCT;                       \
    (OUTPUT).fields[1u].archetype_inline_array.item_archetype_struct.type_name =                                       \
        kan_string_intern (#GENERATED_NODE_TYPE_NAME);                                                                 \
    (OUTPUT).fields[1u].visibility_condition_field = NULL;                                                             \
    (OUTPUT).fields[1u].visibility_condition_values_count = 0u;                                                        \
    (OUTPUT).fields[1u].visibility_condition_values = NULL

/// \brief Helper for reflection generators that provides common way of filling generated mutator deploy function.
#define KAN_UNIVERSE_REFLECTION_GENERATOR_DEPLOY_FUNCTION(OUTPUT, MUTATOR_NAME, MUTATOR_TYPE_NAME, FUNCTION,           \
                                                          ALLOCATION_GROUP)                                            \
    (OUTPUT).name = kan_string_intern ("kan_universe_mutator_deploy_" #MUTATOR_NAME);                                  \
    (OUTPUT).call = FUNCTION;                                                                                          \
    (OUTPUT).call_user_data = 0u;                                                                                      \
                                                                                                                       \
    (OUTPUT).return_type.size = 0u;                                                                                    \
    (OUTPUT).return_type.archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT;                                              \
                                                                                                                       \
    (OUTPUT).arguments_count = 5u;                                                                                     \
    (OUTPUT).arguments = kan_allocate_general (ALLOCATION_GROUP, sizeof (struct kan_reflection_argument_t) * 5u,       \
                                               alignof (struct kan_reflection_argument_t));                            \
                                                                                                                       \
    (OUTPUT).arguments[0u].name = kan_string_intern ("universe");                                                      \
    (OUTPUT).arguments[0u].size = sizeof (kan_universe_t);                                                             \
    (OUTPUT).arguments[0u].archetype = KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL;                                      \
                                                                                                                       \
    (OUTPUT).arguments[1u].name = kan_string_intern ("world");                                                         \
    (OUTPUT).arguments[1u].size = sizeof (kan_universe_world_t);                                                       \
    (OUTPUT).arguments[1u].archetype = KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL;                                      \
                                                                                                                       \
    (OUTPUT).arguments[2u].name = kan_string_intern ("world_repository");                                              \
    (OUTPUT).arguments[2u].size = sizeof (kan_repository_t);                                                           \
    (OUTPUT).arguments[2u].archetype = KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL;                                      \
                                                                                                                       \
    (OUTPUT).arguments[3u].name = kan_string_intern ("workflow_node");                                                 \
    (OUTPUT).arguments[3u].size = sizeof (kan_workflow_graph_node_t);                                                  \
    (OUTPUT).arguments[3u].archetype = KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL;                                      \
                                                                                                                       \
    (OUTPUT).arguments[4u].name = kan_string_intern ("state");                                                         \
    (OUTPUT).arguments[4u].size = sizeof (void *);                                                                     \
    (OUTPUT).arguments[4u].archetype = KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER;                                        \
    (OUTPUT).arguments[4u].archetype_struct_pointer.type_name = kan_string_intern (#MUTATOR_TYPE_NAME)

/// \brief Helper for reflection generators that provides common way of filling generated mutator execute function.
#define KAN_UNIVERSE_REFLECTION_GENERATOR_EXECUTE_FUNCTION(OUTPUT, MUTATOR_NAME, MUTATOR_TYPE_NAME, FUNCTION,          \
                                                           ALLOCATION_GROUP)                                           \
    (OUTPUT).name = kan_string_intern ("kan_universe_mutator_execute_" #MUTATOR_NAME);                                 \
    (OUTPUT).call = FUNCTION;                                                                                          \
    (OUTPUT).call_user_data = 0u;                                                                                      \
                                                                                                                       \
    (OUTPUT).return_type.size = 0u;                                                                                    \
    (OUTPUT).return_type.archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT;                                              \
                                                                                                                       \
    (OUTPUT).arguments_count = 2u;                                                                                     \
    (OUTPUT).arguments = kan_allocate_general (ALLOCATION_GROUP, sizeof (struct kan_reflection_argument_t) * 2u,       \
                                               alignof (struct kan_reflection_argument_t));                            \
                                                                                                                       \
    (OUTPUT).arguments[0u].name = kan_string_intern ("job");                                                           \
    (OUTPUT).arguments[0u].size = sizeof (kan_cpu_job_t);                                                              \
    (OUTPUT).arguments[0u].archetype = KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL;                                      \
                                                                                                                       \
    (OUTPUT).arguments[1u].name = kan_string_intern ("state");                                                         \
    (OUTPUT).arguments[1u].size = sizeof (void *);                                                                     \
    (OUTPUT).arguments[1u].archetype = KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER;                                        \
    (OUTPUT).arguments[1u].archetype_struct_pointer.type_name = kan_string_intern (#MUTATOR_TYPE_NAME)

/// \brief Helper for reflection generators that provides common way of filling generated mutator undeploy function.
#define KAN_UNIVERSE_REFLECTION_GENERATOR_UNDEPLOY_FUNCTION(OUTPUT, MUTATOR_NAME, MUTATOR_TYPE_NAME, FUNCTION,         \
                                                            ALLOCATION_GROUP)                                          \
    (OUTPUT).name = kan_string_intern ("kan_universe_mutator_undeploy_" #MUTATOR_NAME);                                \
    (OUTPUT).call = FUNCTION;                                                                                          \
    (OUTPUT).call_user_data = 0u;                                                                                      \
                                                                                                                       \
    (OUTPUT).return_type.size = 0u;                                                                                    \
    (OUTPUT).return_type.archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT;                                              \
                                                                                                                       \
    (OUTPUT).arguments_count = 1u;                                                                                     \
    (OUTPUT).arguments = kan_allocate_general (ALLOCATION_GROUP, sizeof (struct kan_reflection_argument_t) * 1u,       \
                                               alignof (struct kan_reflection_argument_t));                            \
                                                                                                                       \
    (OUTPUT).arguments[0u].name = kan_string_intern ("state");                                                         \
    (OUTPUT).arguments[0u].size = sizeof (void *);                                                                     \
    (OUTPUT).arguments[0u].archetype = KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER;                                        \
    (OUTPUT).arguments[0u].archetype_struct_pointer.type_name = kan_string_intern (#MUTATOR_TYPE_NAME)

/// \brief Implements simple binary search for appropriate trailing sub-state.
/// \invariant Requires `state` variable to exist.
/// \param STATE_TYPE Type of generated trailing sub-states including `struct` prefix.
/// \param STATE_FIELD Field inside trailing sub-state using for comparison.
/// \param SEARCH_VARIABLE Variable with value for search.
#define KAN_UNIVERSE_REFLECTION_GENERATOR_FIND_GENERATED_STATE(STATE_TYPE, STATE_FIELD, SEARCH_VARIABLE)               \
    kan_loop_size_t left = 0u;                                                                                         \
    kan_loop_size_t right = state->trailing_data_count;                                                                \
    STATE_TYPE *types = (STATE_TYPE *) state->trailing_data;                                                           \
                                                                                                                       \
    while (left < right)                                                                                               \
    {                                                                                                                  \
        kan_loop_size_t middle = (left + right) / 2u;                                                                  \
        if (SEARCH_VARIABLE < types[middle].STATE_FIELD)                                                               \
        {                                                                                                              \
            right = middle;                                                                                            \
        }                                                                                                              \
        else if (SEARCH_VARIABLE > types[middle].STATE_FIELD)                                                          \
        {                                                                                                              \
            left = middle + 1u;                                                                                        \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            return &types[middle];                                                                                     \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    return NULL

KAN_C_HEADER_END
