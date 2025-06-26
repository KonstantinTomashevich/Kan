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

/// \brief Name of the array used in SORT_TYPE_NODES related macros.
#define KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY nodes_array_to_sort

/// \brief Internal macro needed for swaps inside KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES.
#define KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_SWAP(first_index, second_index)                              \
    __CUSHION_PRESERVE__                                                                                               \
    node = KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY[first_index],                                       \
    KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY[first_index] =                                             \
        KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY[second_index],                                         \
    KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY[second_index] = node

/// \brief Convenience macro for sorting generator typed nodes.
/// \invariant User needs to declare KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_LESS with two indices as
///            parameters. Macro should compare two nodes from KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY
///            at given indices as one expression.
/// \param COUNT Expression that returns count of nodes inside generator.
/// \param NODE_TYPE Generator node type including `struct` prefix.
/// \param FIRST_NODE Expression for getting and setting first node in generator.
/// \param ALLOCATION_GROUP Expression that returns allocation group for temporary array.
#define KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES(COUNT, NODE_TYPE, FIRST_NODE, ALLOCATION_GROUP)              \
    NODE_TYPE **KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY =                                              \
        kan_allocate_general (ALLOCATION_GROUP, sizeof (void *) * COUNT, _Alignof (void *));                           \
                                                                                                                       \
    NODE_TYPE *node = FIRST_NODE;                                                                                      \
    kan_loop_size_t output_index = 0u;                                                                                 \
                                                                                                                       \
    while (node)                                                                                                       \
    {                                                                                                                  \
        KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY[output_index] = node;                                  \
        ++output_index;                                                                                                \
        node = node->next;                                                                                             \
    }                                                                                                                  \
                                                                                                                       \
    QSORT ((unsigned long) COUNT, KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_LESS,                              \
           KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_SWAP);                                                    \
                                                                                                                       \
    for (kan_loop_size_t node_index = 0u; node_index < COUNT; ++node_index)                                            \
    {                                                                                                                  \
        if (node_index + 1u < COUNT)                                                                                   \
        {                                                                                                              \
            KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY[node_index]->next =                                \
                KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY[node_index + 1u];                              \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY[node_index]->next = NULL;                          \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    FIRST_NODE = KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY[0u];                                          \
    kan_free_general (ALLOCATION_GROUP, KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY,                       \
                      sizeof (void *) * COUNT)

/// \brief Declares functions to be used for KAN_UNIVERSE_REFLECTION_GENERATOR_FILL_MUTATOR.
/// \param PREFIX Prefix for all generated function names.
/// \param GENERATOR_TYPE Type of reflection generator including `struct` prefix.
/// \param GENERATOR_NODE_TYPE Type of used generator nodes including `struct` prefix.
/// \param GENERATOR_NODE_COUNT Expression that returns number of generator nodes.
/// \param GENERATOR_FIRST_NODE Expression that returns first generator node.
/// \param STATE_TYPE Type of actual mutator state including `struct` prefix.
/// \param GENERATED_STATES_TYPE Type of generated trailing sub-states including `struct` prefix.
/// \param FUNCTION_INIT Function to initialize actual mutator state.
/// \param FUNCTION_DEPLOY Function to deploy actual mutator.
/// \param FUNCTION_EXECUTE Function to execute actual mutator.
/// \param FUNCTION_UNDEPLOY Function to undeploy actual mutator.
/// \param FUNCTION_SHUTDOWN Function to shutdown actual mutator state.
/// \invariant Requires `PREFIX##_init_node` function for initializing trailing sub-state nodes.
/// \invariant Requires `PREFIX##_deploy_node` function for deploying trailing sub-state nodes.
/// \invariant Requires `PREFIX##_undeploy_node` function for undeploying trailing sub-state nodes.
/// \invariant Requires `PREFIX##_shutdown_node` function for shutting down trailing sub-state nodes.
#define KAN_UNIVERSE_REFLECTION_GENERATOR_MUTATOR_FUNCTIONS(                                                           \
    PREFIX, GENERATOR_TYPE, GENERATOR_NODE_TYPE, GENERATOR_NODE_COUNT, GENERATOR_FIRST_NODE, STATE_TYPE,               \
    GENERATED_STATES_TYPE, FUNCTION_INIT, FUNCTION_DEPLOY, FUNCTION_EXECUTE, FUNCTION_UNDEPLOY, FUNCTION_SHUTDOWN)     \
    static void PREFIX##_init (kan_functor_user_data_t function_user_data, void *data)                                 \
    {                                                                                                                  \
        GENERATOR_TYPE *instance = (GENERATOR_TYPE *) function_user_data;                                              \
                                                                                                                       \
        STATE_TYPE *base_state = (STATE_TYPE *) data;                                                                  \
        FUNCTION_INIT (base_state);                                                                                    \
        base_state->trailing_data_count = GENERATOR_NODE_COUNT;                                                        \
                                                                                                                       \
        GENERATOR_NODE_TYPE *node = GENERATOR_FIRST_NODE;                                                              \
        GENERATED_STATES_TYPE *mutator_node = (GENERATED_STATES_TYPE *) base_state->trailing_data;                     \
                                                                                                                       \
        while (node)                                                                                                   \
        {                                                                                                              \
            PREFIX##_init_node (mutator_node, node);                                                                   \
            node = node->next;                                                                                         \
            ++mutator_node;                                                                                            \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    static void PREFIX##_deploy (kan_functor_user_data_t user_data, void *return_address, void *arguments_address)     \
    {                                                                                                                  \
        struct                                                                                                         \
        {                                                                                                              \
            kan_universe_t universe;                                                                                   \
            kan_universe_world_t world;                                                                                \
            kan_repository_t world_repository;                                                                         \
            kan_workflow_graph_node_t workflow_node;                                                                   \
            STATE_TYPE *state;                                                                                         \
        } *arguments = arguments_address;                                                                              \
                                                                                                                       \
        FUNCTION_DEPLOY (arguments->universe, arguments->world, arguments->world_repository, arguments->workflow_node, \
                         arguments->state);                                                                            \
        GENERATED_STATES_TYPE *mutator_nodes = (GENERATED_STATES_TYPE *) arguments->state->trailing_data;              \
                                                                                                                       \
        for (kan_loop_size_t index = 0u; index < arguments->state->trailing_data_count; ++index)                       \
        {                                                                                                              \
            GENERATED_STATES_TYPE *node = &mutator_nodes[index];                                                       \
            PREFIX##_deploy_node (arguments->world_repository, arguments->workflow_node, node);                        \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    static void PREFIX##_execute (kan_functor_user_data_t user_data, void *return_address, void *arguments_address)    \
    {                                                                                                                  \
        struct                                                                                                         \
        {                                                                                                              \
            kan_cpu_job_t job;                                                                                         \
            STATE_TYPE *state;                                                                                         \
        } *arguments = arguments_address;                                                                              \
                                                                                                                       \
        FUNCTION_EXECUTE (arguments->job, arguments->state);                                                           \
    }                                                                                                                  \
                                                                                                                       \
    static void PREFIX##_undeploy (kan_functor_user_data_t user_data, void *return_address, void *arguments_address)   \
    {                                                                                                                  \
        struct                                                                                                         \
        {                                                                                                              \
            STATE_TYPE *state;                                                                                         \
        } *arguments = arguments_address;                                                                              \
                                                                                                                       \
        FUNCTION_UNDEPLOY (arguments->state);                                                                          \
        GENERATED_STATES_TYPE *mutator_nodes = (GENERATED_STATES_TYPE *) arguments->state->trailing_data;              \
                                                                                                                       \
        for (kan_loop_size_t index = 0u; index < arguments->state->trailing_data_count; ++index)                       \
        {                                                                                                              \
            PREFIX##_undeploy_node (&mutator_nodes[index]);                                                            \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    static void PREFIX##_shutdown (kan_functor_user_data_t function_user_data, void *data)                             \
    {                                                                                                                  \
        STATE_TYPE *base_state = (STATE_TYPE *) data;                                                                  \
        FUNCTION_SHUTDOWN (base_state);                                                                                \
                                                                                                                       \
        GENERATED_STATES_TYPE *mutator_nodes = (GENERATED_STATES_TYPE *) base_state->trailing_data;                    \
                                                                                                                       \
        for (kan_loop_size_t index = 0u; index < base_state->trailing_data_count; ++index)                             \
        {                                                                                                              \
            PREFIX##_shutdown_node (&mutator_nodes[index]);                                                            \
        }                                                                                                              \
    }

/// \brief Fills reflection data structures with generated mutator state struct and functions.
/// \param OUTPUT_PREFIX Prefix for output variables, for example `instance->mutator`. There are several outputs:
///                      `OUTPUT_PREFIX##_type` for mutator state struct output. `OUTPUT_PREFIX##_deploy_function` for
///                      deploy function data output. `OUTPUT_PREFIX##_execute_function` for execute function data
///                      output. `OUTPUT_PREFIX##_undeploy_function` for undeploy function data output.
/// \param TYPE_NAME String literal with type name for generate mutator state.
/// \param BASE_STATE_NAME Name of the struct, that is functions as base for mutator state, without `struct` prefix.
/// \param GENERATED_STATES_NAME Name of the struct, which instances will be added as trailing sub-states, without
///                              `struct` prefix.
/// \param GENERATED_STATES_COUNT Expression that returns count of trailing sub-states to generate.
/// \param MUTATOR_NAME Name of generated mutator without quotes.
/// \param FUNCTION_PREFIX Prefix for generated mutator functions, possibly generated using
///                        KAN_UNIVERSE_REFLECTION_GENERATOR_MUTATOR_FUNCTIONS.
#define KAN_UNIVERSE_REFLECTION_GENERATOR_FILL_MUTATOR(OUTPUT_PREFIX, TYPE_NAME, BASE_STATE_NAME,                      \
                                                       GENERATED_STATES_NAME, GENERATED_STATES_COUNT, MUTATOR_NAME,    \
                                                       FUNCTION_PREFIX)                                                \
    OUTPUT_PREFIX##_type.name = kan_string_intern (TYPE_NAME);                                                         \
    OUTPUT_PREFIX##_type.alignment = _Alignof (struct BASE_STATE_NAME);                                                \
    OUTPUT_PREFIX##_type.size =                                                                                        \
        sizeof (struct BASE_STATE_NAME) + sizeof (struct GENERATED_STATES_NAME) * GENERATED_STATES_COUNT;              \
                                                                                                                       \
    OUTPUT_PREFIX##_type.functor_user_data = (kan_functor_user_data_t) instance;                                       \
    OUTPUT_PREFIX##_type.init = FUNCTION_PREFIX##_init;                                                                \
    OUTPUT_PREFIX##_type.shutdown = FUNCTION_PREFIX##_shutdown;                                                        \
                                                                                                                       \
    OUTPUT_PREFIX##_type.fields_count = 2u;                                                                            \
    OUTPUT_PREFIX##_type.fields =                                                                                      \
        kan_allocate_general (instance->generated_reflection_group, sizeof (struct kan_reflection_field_t) * 2u,       \
                              _Alignof (struct kan_reflection_field_t));                                               \
                                                                                                                       \
    OUTPUT_PREFIX##_type.fields[0u].name = kan_string_intern ("base_mutator_state");                                   \
    OUTPUT_PREFIX##_type.fields[0u].offset = 0u;                                                                       \
    OUTPUT_PREFIX##_type.fields[0u].size = sizeof (struct BASE_STATE_NAME);                                            \
    OUTPUT_PREFIX##_type.fields[0u].archetype = KAN_REFLECTION_ARCHETYPE_STRUCT;                                       \
    OUTPUT_PREFIX##_type.fields[0u].archetype_struct.type_name = kan_string_intern (#BASE_STATE_NAME);                 \
    OUTPUT_PREFIX##_type.fields[0u].visibility_condition_field = NULL;                                                 \
    OUTPUT_PREFIX##_type.fields[0u].visibility_condition_values_count = 0u;                                            \
    OUTPUT_PREFIX##_type.fields[0u].visibility_condition_values = NULL;                                                \
                                                                                                                       \
    OUTPUT_PREFIX##_type.fields[1u].name = kan_string_intern ("container_types");                                      \
    OUTPUT_PREFIX##_type.fields[1u].offset = sizeof (struct BASE_STATE_NAME);                                          \
    OUTPUT_PREFIX##_type.fields[1u].size = sizeof (struct GENERATED_STATES_NAME) * GENERATED_STATES_COUNT;             \
    OUTPUT_PREFIX##_type.fields[1u].archetype = KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY;                                 \
    OUTPUT_PREFIX##_type.fields[1u].archetype_inline_array.item_count = GENERATED_STATES_COUNT;                        \
    OUTPUT_PREFIX##_type.fields[1u].archetype_inline_array.item_size = sizeof (struct GENERATED_STATES_NAME);          \
    OUTPUT_PREFIX##_type.fields[1u].archetype_inline_array.size_field = NULL;                                          \
    OUTPUT_PREFIX##_type.fields[1u].archetype_inline_array.item_archetype = KAN_REFLECTION_ARCHETYPE_STRUCT;           \
    OUTPUT_PREFIX##_type.fields[1u].archetype_inline_array.item_archetype_struct.type_name =                           \
        kan_string_intern (#GENERATED_STATES_NAME);                                                                    \
    OUTPUT_PREFIX##_type.fields[1u].visibility_condition_field = NULL;                                                 \
    OUTPUT_PREFIX##_type.fields[1u].visibility_condition_values_count = 0u;                                            \
    OUTPUT_PREFIX##_type.fields[1u].visibility_condition_values = NULL;                                                \
                                                                                                                       \
    OUTPUT_PREFIX##_deploy_function.name = kan_string_intern ("kan_universe_mutator_deploy_" #MUTATOR_NAME);           \
    OUTPUT_PREFIX##_deploy_function.call = FUNCTION_PREFIX##_deploy;                                                   \
    OUTPUT_PREFIX##_deploy_function.call_user_data = 0u;                                                               \
                                                                                                                       \
    OUTPUT_PREFIX##_deploy_function.return_type.size = 0u;                                                             \
    OUTPUT_PREFIX##_deploy_function.return_type.archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT;                       \
                                                                                                                       \
    OUTPUT_PREFIX##_deploy_function.arguments_count = 5u;                                                              \
    OUTPUT_PREFIX##_deploy_function.arguments =                                                                        \
        kan_allocate_general (instance->generated_reflection_group, sizeof (struct kan_reflection_argument_t) * 5u,    \
                              _Alignof (struct kan_reflection_argument_t));                                            \
                                                                                                                       \
    OUTPUT_PREFIX##_deploy_function.arguments[0u].name = kan_string_intern ("universe");                               \
    OUTPUT_PREFIX##_deploy_function.arguments[0u].size = sizeof (kan_universe_t);                                      \
    OUTPUT_PREFIX##_deploy_function.arguments[0u].archetype = KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL;               \
                                                                                                                       \
    OUTPUT_PREFIX##_deploy_function.arguments[1u].name = kan_string_intern ("world");                                  \
    OUTPUT_PREFIX##_deploy_function.arguments[1u].size = sizeof (kan_universe_world_t);                                \
    OUTPUT_PREFIX##_deploy_function.arguments[1u].archetype = KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL;               \
                                                                                                                       \
    OUTPUT_PREFIX##_deploy_function.arguments[2u].name = kan_string_intern ("world_repository");                       \
    OUTPUT_PREFIX##_deploy_function.arguments[2u].size = sizeof (kan_repository_t);                                    \
    OUTPUT_PREFIX##_deploy_function.arguments[2u].archetype = KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL;               \
                                                                                                                       \
    OUTPUT_PREFIX##_deploy_function.arguments[3u].name = kan_string_intern ("workflow_node");                          \
    OUTPUT_PREFIX##_deploy_function.arguments[3u].size = sizeof (kan_workflow_graph_node_t);                           \
    OUTPUT_PREFIX##_deploy_function.arguments[3u].archetype = KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL;               \
                                                                                                                       \
    OUTPUT_PREFIX##_deploy_function.arguments[4u].name = kan_string_intern ("state");                                  \
    OUTPUT_PREFIX##_deploy_function.arguments[4u].size = sizeof (void *);                                              \
    OUTPUT_PREFIX##_deploy_function.arguments[4u].archetype = KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER;                 \
    OUTPUT_PREFIX##_deploy_function.arguments[4u].archetype_struct_pointer.type_name = OUTPUT_PREFIX##_type.name;      \
                                                                                                                       \
    OUTPUT_PREFIX##_execute_function.name = kan_string_intern ("kan_universe_mutator_execute_" #MUTATOR_NAME);         \
    OUTPUT_PREFIX##_execute_function.call = FUNCTION_PREFIX##_execute;                                                 \
    OUTPUT_PREFIX##_execute_function.call_user_data = 0u;                                                              \
                                                                                                                       \
    OUTPUT_PREFIX##_execute_function.return_type.size = 0u;                                                            \
    OUTPUT_PREFIX##_execute_function.return_type.archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT;                      \
                                                                                                                       \
    OUTPUT_PREFIX##_execute_function.arguments_count = 2u;                                                             \
    OUTPUT_PREFIX##_execute_function.arguments =                                                                       \
        kan_allocate_general (instance->generated_reflection_group, sizeof (struct kan_reflection_argument_t) * 2u,    \
                              _Alignof (struct kan_reflection_argument_t));                                            \
                                                                                                                       \
    OUTPUT_PREFIX##_execute_function.arguments[0u].name = kan_string_intern ("job");                                   \
    OUTPUT_PREFIX##_execute_function.arguments[0u].size = sizeof (kan_cpu_job_t);                                      \
    OUTPUT_PREFIX##_execute_function.arguments[0u].archetype = KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL;              \
                                                                                                                       \
    OUTPUT_PREFIX##_execute_function.arguments[1u].name = kan_string_intern ("state");                                 \
    OUTPUT_PREFIX##_execute_function.arguments[1u].size = sizeof (void *);                                             \
    OUTPUT_PREFIX##_execute_function.arguments[1u].archetype = KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER;                \
    OUTPUT_PREFIX##_execute_function.arguments[1u].archetype_struct_pointer.type_name = OUTPUT_PREFIX##_type.name;     \
                                                                                                                       \
    OUTPUT_PREFIX##_undeploy_function.name = kan_string_intern ("kan_universe_mutator_undeploy_" #MUTATOR_NAME);       \
    OUTPUT_PREFIX##_undeploy_function.call = FUNCTION_PREFIX##_undeploy;                                               \
    OUTPUT_PREFIX##_undeploy_function.call_user_data = 0u;                                                             \
                                                                                                                       \
    OUTPUT_PREFIX##_undeploy_function.return_type.size = 0u;                                                           \
    OUTPUT_PREFIX##_undeploy_function.return_type.archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT;                     \
                                                                                                                       \
    OUTPUT_PREFIX##_undeploy_function.arguments_count = 1u;                                                            \
    OUTPUT_PREFIX##_undeploy_function.arguments =                                                                      \
        kan_allocate_general (instance->generated_reflection_group, sizeof (struct kan_reflection_argument_t) * 1u,    \
                              _Alignof (struct kan_reflection_argument_t));                                            \
                                                                                                                       \
    OUTPUT_PREFIX##_undeploy_function.arguments[0u].name = kan_string_intern ("state");                                \
    OUTPUT_PREFIX##_undeploy_function.arguments[0u].size = sizeof (void *);                                            \
    OUTPUT_PREFIX##_undeploy_function.arguments[0u].archetype = KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER;               \
    OUTPUT_PREFIX##_undeploy_function.arguments[0u].archetype_struct_pointer.type_name = OUTPUT_PREFIX##_type.name

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
