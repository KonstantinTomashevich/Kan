#include <stddef.h>
#include <string.h>

#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/render_pipeline_language/emitter.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (rpl_emitter);

struct rpl_emitter_option_value_t
{
    kan_interned_string_t name;
    enum kan_rpl_option_scope_t scope;
    enum kan_rpl_option_type_t type;

    union
    {
        kan_bool_t flag_value;
        uint64_t count_value;
    };
};

struct rpl_emitter_t
{
    kan_interned_string_t log_name;
    enum kan_rpl_pipeline_type_t pipeline_type;

    /// \meta reflection_dynamic_array_type = "struct rpl_emitter_option_value_t"
    struct kan_dynamic_array_t option_values;

    struct kan_rpl_intermediate_t *intermediate;
};

enum conditional_evaluation_result_t
{
    CONDITIONAL_EVALUATION_RESULT_FAILED = 0u,
    CONDITIONAL_EVALUATION_RESULT_TRUE,
    CONDITIONAL_EVALUATION_RESULT_FALSE,
};

enum compile_time_evaluation_value_type_t
{
    CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR = 0u,
    CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN,
    CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER,
    CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING,
};

struct compile_time_evaluation_value_t
{
    enum compile_time_evaluation_value_type_t type;
    union
    {
        kan_bool_t boolean_value;
        int64_t integer_value;
        double floating_value;
    };
};

enum inbuilt_type_item_t
{
    INBUILT_TYPE_ITEM_FLOAT = 0u,
    INBUILT_TYPE_ITEM_INTEGER,
};

struct inbuilt_vector_type_t
{
    kan_interned_string_t name;
    enum inbuilt_type_item_t item;
    uint32_t items_count;
    enum kan_rpl_meta_variable_type_t meta_type;

    uint32_t spirv_id;
};

struct inbuilt_matrix_type_t
{
    kan_interned_string_t name;
    enum inbuilt_type_item_t item;
    uint32_t rows;
    uint32_t columns;
    enum kan_rpl_meta_variable_type_t meta_type;

    uint32_t spirv_id;
};

struct validation_type_info_t
{
    kan_interned_string_t type;

    /// \details Should always point to the list in AST, therefore should not be destroyed.
    struct kan_dynamic_array_t *array_sizes;

    uint64_t array_sizes_offset;
};

struct validation_variable_t
{
    struct validation_variable_t *next;
    kan_interned_string_t name;
    struct validation_type_info_t type;
};

struct validation_scope_t
{
    struct kan_rpl_function_t *function;
    struct validation_scope_t *parent_scope;
    struct validation_variable_t *first_variable;
    struct kan_rpl_expression_node_t *loop_expression;
};

enum spirv_fixed_ids_t
{
    SPIRV_FIXED_ID_INVALID = 0u,

    SPIRV_FIXED_ID_TYPE_VOID = 1u,
    SPIRV_FIXED_ID_TYPE_BOOLEAN,
    SPIRV_FIXED_ID_TYPE_FLOAT,
    SPIRV_FIXED_ID_TYPE_INTEGER,

    SPIRV_FIXED_ID_TYPE_F2,
    SPIRV_FIXED_ID_TYPE_F3,
    SPIRV_FIXED_ID_TYPE_F4,

    SPIRV_FIXED_ID_TYPE_I2,
    SPIRV_FIXED_ID_TYPE_I3,
    SPIRV_FIXED_ID_TYPE_I4,

    SPIRV_FIXED_ID_TYPE_F3X3,
    SPIRV_FIXED_ID_TYPE_F4X4,

    SPIRV_FIXED_ID_GLSL_LIBRARY,

    SPIRV_FIXED_ID_END,
};

struct spirv_arbitrary_instruction_item_t
{
    struct spirv_arbitrary_instruction_item_t *next;
    uint32_t code[];
};

struct spirv_arbitrary_instruction_section_t
{
    struct spirv_arbitrary_instruction_item_t *first;
    struct spirv_arbitrary_instruction_item_t *last;
};

struct spirv_struct_type_t
{
    struct spirv_struct_type_t *next;
    struct kan_rpl_struct_t *struct_data;
    uint32_t type_id;

    /// \details Invalid ids for fields that aren't allowed by conditionals.
    uint32_t field_ids[];
};

struct spirv_buffer_id_t
{
    struct spirv_buffer_id_t *next;
    struct kan_rpl_buffer_t *buffer;

    /// \details Single buffer variable id if collapsed buffer, ids of all fields if unwrapped buffer,
    ///          filtered out fields have invalid id in this case.
    uint32_t ids[];
};

struct spirv_function_id_t
{
    struct spirv_function_id_t *next;
    struct kan_rpl_function_t *function;
    uint32_t id;
};

struct spirv_generation_context_t
{
    uint32_t current_bound;
    uint32_t code_word_count;
    kan_bool_t emit_result;
    enum kan_rpl_pipeline_stage_t stage;

    struct spirv_struct_type_t *first_struct_id;
    struct spirv_buffer_id_t *first_buffer_id;
    struct spirv_function_id_t *first_function_id;

    struct spirv_arbitrary_instruction_section_t debug_section;
    struct spirv_arbitrary_instruction_section_t annotation_section;
    struct spirv_arbitrary_instruction_section_t type_section;
    struct spirv_arbitrary_instruction_section_t global_variable_section;
    struct spirv_arbitrary_instruction_section_t functions_section;

    struct kan_stack_group_allocator_t temporary_allocator;
};

static kan_bool_t statics_initialized = KAN_FALSE;
static kan_allocation_group_t rpl_allocation_group;
static kan_allocation_group_t rpl_meta_allocation_group;
static kan_allocation_group_t rpl_emitter_allocation_group;
static kan_allocation_group_t rpl_emitter_validation_allocation_group;
static kan_allocation_group_t rpl_emitter_generation_allocation_group;

static kan_interned_string_t interned_fill;
static kan_interned_string_t interned_wireframe;
static kan_interned_string_t interned_back;

static kan_interned_string_t interned_polygon_mode;
static kan_interned_string_t interned_cull_mode;
static kan_interned_string_t interned_depth_test;
static kan_interned_string_t interned_depth_write;

static kan_interned_string_t interned_nearest;
static kan_interned_string_t interned_linear;
static kan_interned_string_t interned_repeat;
static kan_interned_string_t interned_mirrored_repeat;
static kan_interned_string_t interned_clamp_to_edge;
static kan_interned_string_t interned_clamp_to_border;
static kan_interned_string_t interned_mirror_clamp_to_edge;
static kan_interned_string_t interned_mirror_clamp_to_border;

static kan_interned_string_t interned_mag_filter;
static kan_interned_string_t interned_min_filter;
static kan_interned_string_t interned_mip_map_mode;
static kan_interned_string_t interned_address_mode_u;
static kan_interned_string_t interned_address_mode_v;
static kan_interned_string_t interned_address_mode_w;

static kan_interned_string_t interned_void;
static kan_interned_string_t interned_bool;

#define INBUILT_ELEMENT_IDENTIFIERS_ITEMS 4u
#define INBUILT_ELEMENT_IDENTIFIERS_VARIANTS 2u

static char inbuilt_element_identifiers[INBUILT_ELEMENT_IDENTIFIERS_ITEMS][INBUILT_ELEMENT_IDENTIFIERS_VARIANTS] = {
    {'x', 'r'}, {'y', 'g'}, {'z', 'b'}, {'w', 'a'}};

struct inbuilt_vector_type_t type_f1;
struct inbuilt_vector_type_t type_f2;
struct inbuilt_vector_type_t type_f3;
struct inbuilt_vector_type_t type_f4;
struct inbuilt_vector_type_t type_i1;
struct inbuilt_vector_type_t type_i2;
struct inbuilt_vector_type_t type_i3;
struct inbuilt_vector_type_t type_i4;
struct inbuilt_vector_type_t *vector_types[] = {&type_f1, &type_f2, &type_f3, &type_f4,
                                                &type_i1, &type_i2, &type_i3, &type_i4};
struct inbuilt_vector_type_t *floating_vector_types[] = {&type_f1, &type_f2, &type_f3, &type_f4};
struct inbuilt_vector_type_t *integer_vector_types[] = {&type_i1, &type_i2, &type_i3, &type_i4};

struct inbuilt_matrix_type_t type_f3x3;
struct inbuilt_matrix_type_t type_f4x4;
struct inbuilt_matrix_type_t *matrix_types[] = {&type_f3x3, &type_f4x4};

static inline void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        rpl_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "render_pipeline_language");
        rpl_meta_allocation_group = kan_allocation_group_get_child (rpl_allocation_group, "meta");
        rpl_emitter_allocation_group = kan_allocation_group_get_child (rpl_allocation_group, "emitter");
        rpl_emitter_validation_allocation_group =
            kan_allocation_group_get_child (rpl_emitter_allocation_group, "validation");
        rpl_emitter_generation_allocation_group =
            kan_allocation_group_get_child (rpl_emitter_allocation_group, "generation");

        interned_fill = kan_string_intern ("fill");
        interned_wireframe = kan_string_intern ("wireframe");
        interned_back = kan_string_intern ("back");

        interned_polygon_mode = kan_string_intern ("polygon_mode");
        interned_cull_mode = kan_string_intern ("cull_mode");
        interned_depth_test = kan_string_intern ("depth_test");
        interned_depth_write = kan_string_intern ("depth_write");

        interned_nearest = kan_string_intern ("nearest");
        interned_linear = kan_string_intern ("linear");
        interned_repeat = kan_string_intern ("repeat");
        interned_mirrored_repeat = kan_string_intern ("mirrored_repeat");
        interned_clamp_to_edge = kan_string_intern ("clamp_to_edge");
        interned_clamp_to_border = kan_string_intern ("clamp_to_border");
        interned_mirror_clamp_to_edge = kan_string_intern ("mirror_clamp_to_edge");
        interned_mirror_clamp_to_border = kan_string_intern ("mirror_clamp_to_border");

        interned_mag_filter = kan_string_intern ("mag_filter");
        interned_min_filter = kan_string_intern ("min_filter");
        interned_mip_map_mode = kan_string_intern ("mip_map_mode");
        interned_address_mode_u = kan_string_intern ("address_mode_u");
        interned_address_mode_v = kan_string_intern ("address_mode_v");
        interned_address_mode_w = kan_string_intern ("address_mode_w");

        interned_void = kan_string_intern ("void");
        interned_bool = kan_string_intern ("bool");

        type_f1 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("f1"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .items_count = 1u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F1,
            .spirv_id = SPIRV_FIXED_ID_TYPE_FLOAT,
        };

        type_f2 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("f2"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .items_count = 2u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F2,
            .spirv_id = SPIRV_FIXED_ID_TYPE_F2,
        };

        type_f3 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("f3"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .items_count = 3u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F3,
            .spirv_id = SPIRV_FIXED_ID_TYPE_F3,
        };

        type_f4 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("f4"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .items_count = 4u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F4,
            .spirv_id = SPIRV_FIXED_ID_TYPE_F4,
        };

        type_i1 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("i1"),
            .item = INBUILT_TYPE_ITEM_INTEGER,
            .items_count = 1u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_I1,
            .spirv_id = SPIRV_FIXED_ID_TYPE_INTEGER,
        };

        type_i2 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("i2"),
            .item = INBUILT_TYPE_ITEM_INTEGER,
            .items_count = 2u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_I2,
            .spirv_id = SPIRV_FIXED_ID_TYPE_I2,
        };

        type_i3 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("i3"),
            .item = INBUILT_TYPE_ITEM_INTEGER,
            .items_count = 3u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_I3,
            .spirv_id = SPIRV_FIXED_ID_TYPE_I3,
        };

        type_i4 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("i4"),
            .item = INBUILT_TYPE_ITEM_INTEGER,
            .items_count = 4u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_I4,
            .spirv_id = SPIRV_FIXED_ID_TYPE_I4,
        };

        type_f3x3 = (struct inbuilt_matrix_type_t) {
            .name = kan_string_intern ("f3x3"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .rows = 3u,
            .columns = 3u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F3X3,
            .spirv_id = SPIRV_FIXED_ID_TYPE_F3X3,
        };

        type_f4x4 = (struct inbuilt_matrix_type_t) {
            .name = kan_string_intern ("f4x4"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .rows = 4u,
            .columns = 4u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F4X4,
            .spirv_id = SPIRV_FIXED_ID_TYPE_F4X4,
        };

        statics_initialized = KAN_TRUE;
    }
}

static kan_bool_t inbuilt_function_library_initialized = KAN_FALSE;
static struct kan_atomic_int_t inbuilt_function_library_initialization_lock = {0};
static struct kan_rpl_intermediate_t inbuilt_function_library_intermediate;

static inline void ensure_inbuilt_function_library_initialized (void)
{
    if (!inbuilt_function_library_initialized)
    {
        kan_atomic_int_lock (&inbuilt_function_library_initialization_lock);
        if (!inbuilt_function_library_initialized)
        {
            static const char *library_source =
                "f1 sqrt (f1 value) { sqrt; }"
                "void vertex_stage_output_position (f4 position) {}";

            kan_rpl_parser_t parser = kan_rpl_parser_create (kan_string_intern ("inbuilt_function_library"));
            kan_rpl_parser_add_source (parser, library_source, kan_string_intern ("inbuilt_function_library"));
            kan_rpl_intermediate_init (&inbuilt_function_library_intermediate);
            kan_rpl_parser_build_intermediate (parser, &inbuilt_function_library_intermediate);
            kan_rpl_parser_destroy (parser);
        }

        kan_atomic_int_unlock (&inbuilt_function_library_initialization_lock);
    }
}

static uint64_t find_inbuilt_element_index_by_identifier_character (uint64_t max_elements, char identifier)
{
    const uint64_t items_to_check = KAN_MIN (INBUILT_ELEMENT_IDENTIFIERS_ITEMS, max_elements);
    for (uint64_t element_index = 0u; element_index < items_to_check; ++element_index)
    {
        for (uint64_t variant_index = 0u; variant_index < INBUILT_ELEMENT_IDENTIFIERS_VARIANTS; ++variant_index)
        {
            if (inbuilt_element_identifiers[element_index][variant_index] == identifier)
            {
                return element_index;
            }
        }
    }

    return UINT64_MAX;
}

kan_rpl_emitter_t kan_rpl_emitter_create (kan_interned_string_t log_name,
                                          enum kan_rpl_pipeline_type_t pipeline_type,
                                          struct kan_rpl_intermediate_t *intermediate)
{
    ensure_statics_initialized ();
    ensure_inbuilt_function_library_initialized ();
    struct rpl_emitter_t *emitter = kan_allocate_general (rpl_emitter_allocation_group, sizeof (struct rpl_emitter_t),
                                                          _Alignof (struct rpl_emitter_t));

    emitter->log_name = log_name;
    emitter->pipeline_type = pipeline_type;
    kan_dynamic_array_init (&emitter->option_values, intermediate->options.size,
                            sizeof (struct rpl_emitter_option_value_t), _Alignof (struct rpl_emitter_option_value_t),
                            rpl_emitter_allocation_group);
    emitter->intermediate = intermediate;

    // We copy all options from intermediate for ease of access.
    for (uint64_t option_index = 0u; option_index < intermediate->options.size; ++option_index)
    {
        struct kan_rpl_option_t *source_option =
            &((struct kan_rpl_option_t *) intermediate->options.data)[option_index];
        struct rpl_emitter_option_value_t *target_option = kan_dynamic_array_add_last (&emitter->option_values);
        KAN_ASSERT (target_option)

        target_option->name = source_option->name;
        target_option->scope = source_option->scope;
        target_option->type = source_option->type;

        switch (source_option->type)
        {
        case KAN_RPL_OPTION_TYPE_FLAG:
            target_option->flag_value = source_option->flag_default_value;
            break;

        case KAN_RPL_OPTION_TYPE_COUNT:
            target_option->count_value = source_option->count_default_value;
            break;
        }
    }

    return (kan_rpl_emitter_t) emitter;
}

kan_bool_t kan_rpl_emitter_set_flag_option (kan_rpl_emitter_t emitter, kan_interned_string_t name, kan_bool_t value)
{
    struct rpl_emitter_t *instance = (struct rpl_emitter_t *) emitter;
    for (uint64_t option_index = 0u; option_index < instance->option_values.size; ++option_index)
    {
        struct rpl_emitter_option_value_t *option =
            &((struct rpl_emitter_option_value_t *) instance->option_values.data)[option_index];

        if (option->name == name)
        {
            if (option->type != KAN_RPL_OPTION_TYPE_FLAG)
            {
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s] Option \"%s\" is not a flag.", instance->log_name, name)
                return KAN_FALSE;
            }

            option->flag_value = value;
            return KAN_TRUE;
        }
    }

    KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s] Unable to find option \"%s\".", instance->log_name, name)
    return KAN_FALSE;
}

kan_bool_t kan_rpl_emitter_set_count_option (kan_rpl_emitter_t emitter, kan_interned_string_t name, uint64_t value)
{
    struct rpl_emitter_t *instance = (struct rpl_emitter_t *) emitter;
    for (uint64_t option_index = 0u; option_index < instance->option_values.size; ++option_index)
    {
        struct rpl_emitter_option_value_t *option =
            &((struct rpl_emitter_option_value_t *) instance->option_values.data)[option_index];

        if (option->name == name)
        {
            if (option->type != KAN_RPL_OPTION_TYPE_COUNT)
            {
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s] Option \"%s\" is not a count.", instance->log_name, name)
                return KAN_FALSE;
            }

            option->count_value = value;
            return KAN_TRUE;
        }
    }

    KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s] Unable to find option \"%s\".", instance->log_name, name)
    return KAN_FALSE;
}

static struct compile_time_evaluation_value_t evaluate_compile_time_expression (
    struct rpl_emitter_t *instance, struct kan_rpl_expression_node_t *expression, kan_bool_t instance_options_allowed)
{
    struct compile_time_evaluation_value_t result;
    switch (expression->type)
    {
    case KAN_RPL_EXPRESSION_NODE_TYPE_NOPE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION:
    case KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_IF:
    case KAN_RPL_EXPRESSION_NODE_TYPE_FOR:
    case KAN_RPL_EXPRESSION_NODE_TYPE_WHILE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS:
    case KAN_RPL_EXPRESSION_NODE_TYPE_BREAK:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONTINUE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_RETURN:
        // Should not be allowed by parser.
        KAN_ASSERT (KAN_FALSE)
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER:
    {
        kan_bool_t found = KAN_FALSE;
        for (uint64_t option_index = 0u; option_index < instance->option_values.size; ++option_index)
        {
            struct rpl_emitter_option_value_t *option =
                &((struct rpl_emitter_option_value_t *) instance->option_values.data)[option_index];

            if (option->name == expression->identifier)
            {
                found = KAN_TRUE;
                if (option->scope == KAN_RPL_OPTION_SCOPE_INSTANCE && !instance_options_allowed)
                {
                    KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                             "[%s:%s] [%ld] Compile time expression contains non-global option \"%s\" in context that "
                             "only allows global options.",
                             instance->log_name, expression->source_name, (long) expression->source_line,
                             expression->identifier)
                    result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
                }
                else
                {
                    switch (option->type)
                    {
                    case KAN_RPL_OPTION_TYPE_FLAG:
                        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN;
                        result.boolean_value = option->flag_value;
                        break;

                    case KAN_RPL_OPTION_TYPE_COUNT:
                        if (option->count_value > INT64_MAX)
                        {
                            KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                                     "[%s:%s] [%ld] Compile time expression uses count option \"%s\" that has value "
                                     "%llu that is greater that supported in conditionals.",
                                     instance->log_name, expression->source_name, (long) expression->source_line,
                                     expression->identifier, (unsigned long long) option->count_value)
                            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
                        }
                        else
                        {
                            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER;
                            result.integer_value = (int64_t) option->count_value;
                        }
                        break;
                    }
                }

                break;
            }
        }

        if (!found)
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                     "[%s:%s] [%ld] Compile time expression uses option \"%s\" that cannot be found.",
                     instance->log_name, expression->source_name, (long) expression->source_line,
                     expression->identifier)
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
        }

        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_INTEGER_LITERAL:
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER;
        result.integer_value = expression->integer_literal;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL:
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING;
        result.floating_value = expression->floating_literal;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION:
    {
        struct compile_time_evaluation_value_t left_operand = evaluate_compile_time_expression (
            instance, &((struct kan_rpl_expression_node_t *) expression->children.data)[0u], instance_options_allowed);
        struct compile_time_evaluation_value_t right_operand = evaluate_compile_time_expression (
            instance, &((struct kan_rpl_expression_node_t *) expression->children.data)[1u], instance_options_allowed);

        if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR ||
            right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR)
        {
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            break;
        }

#define EVALUATE_NUMERIC_OPERATION(OPERATOR)                                                                           \
    if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER &&                                              \
        right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)                                               \
    {                                                                                                                  \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER;                                                       \
        result.integer_value = left_operand.integer_value OPERATOR right_operand.integer_value;                        \
    }                                                                                                                  \
    else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING &&                                        \
             right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING)                                         \
    {                                                                                                                  \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING;                                                      \
        result.floating_value = left_operand.floating_value OPERATOR right_operand.floating_value;                     \
    }                                                                                                                  \
    else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER &&                                         \
             right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING)                                         \
    {                                                                                                                  \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING;                                                      \
        result.floating_value = (double) left_operand.integer_value OPERATOR right_operand.floating_value;             \
    }                                                                                                                  \
    else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING &&                                        \
             right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)                                          \
    {                                                                                                                  \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING;                                                      \
        result.floating_value = left_operand.floating_value OPERATOR (double) right_operand.integer_value;             \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Operator \"%s\" has unsupported operand types.",           \
                 instance->log_name, expression->source_name, (long) expression->source_line, #OPERATOR)               \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;                                                         \
    }

#define EVALUATE_COMPARISON_OPERATION(OPERATOR)                                                                        \
    result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN;                                                           \
    if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER &&                                              \
        right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)                                               \
    {                                                                                                                  \
        result.boolean_value = left_operand.integer_value OPERATOR right_operand.integer_value;                        \
    }                                                                                                                  \
    else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING &&                                        \
             right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING)                                         \
    {                                                                                                                  \
        result.boolean_value = left_operand.floating_value OPERATOR right_operand.floating_value;                      \
    }                                                                                                                  \
    else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER &&                                         \
             right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING)                                         \
    {                                                                                                                  \
        result.boolean_value = (double) left_operand.integer_value OPERATOR right_operand.floating_value;              \
    }                                                                                                                  \
    else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING &&                                        \
             right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)                                          \
    {                                                                                                                  \
        result.boolean_value = left_operand.floating_value OPERATOR (double) right_operand.integer_value;              \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Operator \"%s\" has unsupported operand types.",           \
                 instance->log_name, expression->source_name, (long) expression->source_line, #OPERATOR)               \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;                                                         \
    }

#define EVALUATE_BITWISE_OPERATION(OPERATOR)                                                                           \
    if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER &&                                              \
        right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)                                               \
    {                                                                                                                  \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER;                                                       \
        result.integer_value = left_operand.integer_value OPERATOR right_operand.integer_value;                        \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Operator \"%s\" has unsupported operand types.",           \
                 instance->log_name, expression->source_name, (long) expression->source_line, #OPERATOR)               \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;                                                         \
    }

        switch (expression->binary_operation)
        {
        case KAN_RPL_BINARY_OPERATION_FIELD_ACCESS:
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                     "[%s:%s] [%ld] Operator \".\" is not supported in compile time expressions.", instance->log_name,
                     expression->source_name, (long) expression->source_line)
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            break;

        case KAN_RPL_BINARY_OPERATION_ARRAY_ACCESS:
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                     "[%s:%s] [%ld] Operator \"[]\" is not supported in compile time expressions.", instance->log_name,
                     expression->source_name, (long) expression->source_line)
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            break;

        case KAN_RPL_BINARY_OPERATION_ADD:
            EVALUATE_NUMERIC_OPERATION (+)
            break;

        case KAN_RPL_BINARY_OPERATION_SUBTRACT:
            EVALUATE_NUMERIC_OPERATION (-)
            break;

        case KAN_RPL_BINARY_OPERATION_MULTIPLY:
            EVALUATE_NUMERIC_OPERATION (*)
            break;

        case KAN_RPL_BINARY_OPERATION_DIVIDE:
            EVALUATE_NUMERIC_OPERATION (/)
            break;

        case KAN_RPL_BINARY_OPERATION_MODULUS:
            if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER &&
                right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)
            {
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER;
                result.integer_value = left_operand.integer_value % right_operand.integer_value;
            }
            else
            {
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Operator \"%%\" has unsupported operand types.",
                         instance->log_name, expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            }

            break;

        case KAN_RPL_BINARY_OPERATION_ASSIGN:
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Operator \"=\" is not supported in conditionals.",
                     instance->log_name, expression->source_name, (long) expression->source_line)
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            break;

        case KAN_RPL_BINARY_OPERATION_AND:
            if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN &&
                right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN)
            {
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN;
                result.boolean_value = left_operand.boolean_value && right_operand.boolean_value;
            }
            else
            {
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Operator \"&&\" has unsupported operand types.",
                         instance->log_name, expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            }

            break;

        case KAN_RPL_BINARY_OPERATION_OR:
            if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN &&
                right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN)
            {
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN;
                result.boolean_value = left_operand.boolean_value || right_operand.boolean_value;
            }
            else
            {
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Operator \"||\" has unsupported operand types.",
                         instance->log_name, expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            }

            break;

        case KAN_RPL_BINARY_OPERATION_EQUAL:
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN;
            if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER &&
                right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)
            {
                result.boolean_value = left_operand.integer_value == right_operand.integer_value;
            }
            else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN &&
                     right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN)
            {
                result.boolean_value = left_operand.boolean_value == right_operand.boolean_value;
            }
            else
            {
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Operator \"==\" has unsupported operand types.",
                         instance->log_name, expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            }

            break;

        case KAN_RPL_BINARY_OPERATION_NOT_EQUAL:
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN;
            if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER &&
                right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)
            {
                result.boolean_value = left_operand.integer_value != right_operand.integer_value;
            }
            else if (left_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN &&
                     right_operand.type == CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN)
            {
                result.boolean_value = left_operand.boolean_value != right_operand.boolean_value;
            }
            else
            {
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Operator \"!=\" has unsupported operand types.",
                         instance->log_name, expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            }

            break;

        case KAN_RPL_BINARY_OPERATION_LESS:
            EVALUATE_COMPARISON_OPERATION (<)
            break;

        case KAN_RPL_BINARY_OPERATION_GREATER:
            EVALUATE_COMPARISON_OPERATION (>)
            break;

        case KAN_RPL_BINARY_OPERATION_LESS_OR_EQUAL:
            EVALUATE_COMPARISON_OPERATION (<=)
            break;

        case KAN_RPL_BINARY_OPERATION_GREATER_OR_EQUAL:
            EVALUATE_COMPARISON_OPERATION (>=)
            break;

        case KAN_RPL_BINARY_OPERATION_BITWISE_AND:
            EVALUATE_BITWISE_OPERATION (&)
            break;

        case KAN_RPL_BINARY_OPERATION_BITWISE_OR:
            EVALUATE_BITWISE_OPERATION (|)
            break;

        case KAN_RPL_BINARY_OPERATION_BITWISE_XOR:
            EVALUATE_BITWISE_OPERATION (^)
            break;

        case KAN_RPL_BINARY_OPERATION_BITWISE_LSHIFT:
            EVALUATE_BITWISE_OPERATION (<<)
            break;

        case KAN_RPL_BINARY_OPERATION_BITWISE_RSHIFT:
            EVALUATE_BITWISE_OPERATION (>>)
            break;
        }

#undef EVALUATE_NUMERIC_OPERATION
#undef EVALUATE_COMPARISON_OPERATION
#undef EVALUATE_BITWISE_OPERATION

        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION:
    {
        struct compile_time_evaluation_value_t operand = evaluate_compile_time_expression (
            instance, &((struct kan_rpl_expression_node_t *) expression->children.data)[0u], instance_options_allowed);
        result.type = operand.type;

        switch (expression->unary_operation)
        {
        case KAN_RPL_UNARY_OPERATION_NEGATE:
            switch (operand.type)
            {
            case CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR:
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN:
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Operator \"-\" cannot be applied to boolean value.",
                         instance->log_name, expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER:
                result.integer_value = -operand.integer_value;
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING:
                result.floating_value = -result.floating_value;
                break;
            }

            break;

        case KAN_RPL_UNARY_OPERATION_NOT:
            switch (operand.type)
            {
            case CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR:
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN:
                result.boolean_value = !operand.boolean_value;
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER:
            case CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING:
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                         "[%s:%s] [%ld] Operator \"!\" can only be applied to boolean value.", instance->log_name,
                         expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
                break;
            }

            break;

        case KAN_RPL_UNARY_OPERATION_BITWISE_NOT:
            switch (operand.type)
            {
            case CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR:
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER:
                result.integer_value = ~operand.integer_value;
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN:
            case CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING:
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                         "[%s:%s] [%ld] Operator \"~\" can only be applied to integer value.", instance->log_name,
                         expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
                break;
            }

            break;
        }

        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL:
        KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                 "[%s:%s] [%ld] Compile time expression contains function call which is not supported.",
                 instance->log_name, expression->source_name, (long) expression->source_line)
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR:
        KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                 "[%s:%s] [%ld] Compile time expression contains constructor which is not supported.",
                 instance->log_name, expression->source_name, (long) expression->source_line)
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
        break;
    }

    return result;
}

static enum conditional_evaluation_result_t evaluate_conditional (struct rpl_emitter_t *instance,
                                                                  struct kan_rpl_expression_node_t *expression,
                                                                  kan_bool_t instance_options_allowed)
{
    if (expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    {
        return CONDITIONAL_EVALUATION_RESULT_TRUE;
    }

    struct compile_time_evaluation_value_t result =
        evaluate_compile_time_expression (instance, expression, instance_options_allowed);

    switch (result.type)
    {
    case CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR:
        return CONDITIONAL_EVALUATION_RESULT_FAILED;

    case CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN:
        return result.boolean_value ? CONDITIONAL_EVALUATION_RESULT_TRUE : CONDITIONAL_EVALUATION_RESULT_FALSE;

    case CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER:
        return result.integer_value != 0 ? CONDITIONAL_EVALUATION_RESULT_TRUE : CONDITIONAL_EVALUATION_RESULT_FALSE;

    case CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING:
        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Conditional evaluation resulted in floating value.",
                 instance->log_name, expression->source_name, (long) expression->source_line)
        return CONDITIONAL_EVALUATION_RESULT_FAILED;
    }

    KAN_ASSERT (KAN_FALSE)
    return CONDITIONAL_EVALUATION_RESULT_FAILED;
}

static const char *get_setting_type_name (enum kan_rpl_setting_type_t type)
{
    switch (type)
    {
    case KAN_RPL_SETTING_TYPE_FLAG:
        return "flag";

    case KAN_RPL_SETTING_TYPE_INTEGER:
        return "integer";

    case KAN_RPL_SETTING_TYPE_FLOATING:
        return "floating";

    case KAN_RPL_SETTING_TYPE_STRING:
        return "string";
    }

    KAN_ASSERT (KAN_FALSE)
    return "<unknown>";
}

static kan_bool_t validate_setting_type (struct rpl_emitter_t *instance,
                                         struct kan_rpl_setting_t *setting,
                                         enum kan_rpl_setting_type_t expected)
{
    if (setting->type != expected)
    {
        KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                 "[%s:%s] [%ld] Global setting \"%s\" has type \"%s\", but should have type \"%s\".",
                 instance->log_name, setting->source_name, (long) setting->source_line, setting->name,
                 get_setting_type_name (setting->type), get_setting_type_name (expected))
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static kan_bool_t validate_global_setting (struct rpl_emitter_t *instance, struct kan_rpl_setting_t *setting)
{
    enum conditional_evaluation_result_t condition = evaluate_conditional (instance, &setting->conditional, KAN_TRUE);
    switch (condition)
    {
    case CONDITIONAL_EVALUATION_RESULT_FAILED:
        return KAN_FALSE;

    case CONDITIONAL_EVALUATION_RESULT_TRUE:
        break;

    case CONDITIONAL_EVALUATION_RESULT_FALSE:
        return KAN_TRUE;
    }

    if (setting->name == interned_polygon_mode && instance->pipeline_type == KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC)
    {
        if (!validate_setting_type (instance, setting, KAN_RPL_SETTING_TYPE_STRING))
        {
            return KAN_FALSE;
        }

        if (setting->string != interned_fill && setting->string != interned_wireframe)
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Global setting \"%s\" has unsupported value \"%s\".",
                     instance->log_name, setting->source_name, (long) setting->source_line, setting->name,
                     setting->string)
            return KAN_FALSE;
        }
    }
    else if (setting->name == interned_cull_mode && instance->pipeline_type == KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC)
    {
        if (!validate_setting_type (instance, setting, KAN_RPL_SETTING_TYPE_STRING))
        {
            return KAN_FALSE;
        }

        if (setting->string != interned_back)
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Global setting \"%s\" has unsupported value \"%s\".",
                     instance->log_name, setting->source_name, (long) setting->source_line, setting->name,
                     setting->string)
            return KAN_FALSE;
        }
    }
    else if (setting->name == interned_depth_test && instance->pipeline_type == KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC)
    {
        if (!validate_setting_type (instance, setting, KAN_RPL_SETTING_TYPE_FLAG))
        {
            return KAN_FALSE;
        }
    }
    else if (setting->name == interned_depth_write && instance->pipeline_type == KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC)
    {
        if (!validate_setting_type (instance, setting, KAN_RPL_SETTING_TYPE_FLAG))
        {
            return KAN_FALSE;
        }
    }
    else
    {
        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s] Found unknown global setting \"%s\".", instance->log_name,
                 setting->name)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline struct inbuilt_vector_type_t *find_inbuilt_vector_type (kan_interned_string_t name)
{
    for (uint64_t index = 0u; index < sizeof (vector_types) / sizeof (vector_types[0u]); ++index)
    {
        if (vector_types[index]->name == name)
        {
            return vector_types[index];
        }
    }

    return NULL;
}

static inline struct inbuilt_matrix_type_t *find_inbuilt_matrix_type (kan_interned_string_t name)
{
    for (uint64_t index = 0u; index < sizeof (matrix_types) / sizeof (matrix_types[0u]); ++index)
    {
        if (matrix_types[index]->name == name)
        {
            return matrix_types[index];
        }
    }

    return NULL;
}

static struct kan_rpl_option_t *rpl_emitter_find_option (struct rpl_emitter_t *instance, kan_interned_string_t name)
{
    for (uint64_t option_index = 0u; option_index < instance->intermediate->options.size; ++option_index)
    {
        struct kan_rpl_option_t *option =
            &((struct kan_rpl_option_t *) instance->intermediate->options.data)[option_index];

        if (option->name == name)
        {
            return option;
        }
    }

    return NULL;
}

static struct kan_rpl_struct_t *rpl_emitter_find_struct (struct rpl_emitter_t *instance, kan_interned_string_t name)
{
    for (uint64_t struct_index = 0u; struct_index < instance->intermediate->structs.size; ++struct_index)
    {
        struct kan_rpl_struct_t *struct_data =
            &((struct kan_rpl_struct_t *) instance->intermediate->structs.data)[struct_index];

        enum conditional_evaluation_result_t condition =
            evaluate_conditional (instance, &struct_data->conditional, KAN_FALSE);

        switch (condition)
        {
        case CONDITIONAL_EVALUATION_RESULT_TRUE:
            if (struct_data->name == name)
            {
                return struct_data;
            }

            break;

        case CONDITIONAL_EVALUATION_RESULT_FAILED:
        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            break;
        }
    }

    return NULL;
}

static struct kan_rpl_buffer_t *rpl_emitter_find_buffer (struct rpl_emitter_t *instance, kan_interned_string_t name)
{
    for (uint64_t buffer_index = 0u; buffer_index < instance->intermediate->buffers.size; ++buffer_index)
    {
        struct kan_rpl_buffer_t *buffer =
            &((struct kan_rpl_buffer_t *) instance->intermediate->buffers.data)[buffer_index];

        enum conditional_evaluation_result_t condition =
            evaluate_conditional (instance, &buffer->conditional, KAN_FALSE);

        switch (condition)
        {
        case CONDITIONAL_EVALUATION_RESULT_TRUE:
            if (buffer->name == name)
            {
                return buffer;
            }

            break;

        case CONDITIONAL_EVALUATION_RESULT_FAILED:
        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            break;
        }
    }

    return NULL;
}

static struct kan_rpl_sampler_t *rpl_emitter_find_sampler (struct rpl_emitter_t *instance, kan_interned_string_t name)
{
    for (uint64_t sampler_index = 0u; sampler_index < instance->intermediate->samplers.size; ++sampler_index)
    {
        struct kan_rpl_sampler_t *sampler =
            &((struct kan_rpl_sampler_t *) instance->intermediate->samplers.data)[sampler_index];

        enum conditional_evaluation_result_t condition =
            evaluate_conditional (instance, &sampler->conditional, KAN_FALSE);

        switch (condition)
        {
        case CONDITIONAL_EVALUATION_RESULT_TRUE:
            if (sampler->name == name)
            {
                return sampler;
            }

            break;

        case CONDITIONAL_EVALUATION_RESULT_FAILED:
        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            break;
        }
    }

    return NULL;
}

static struct kan_rpl_function_t *rpl_emitter_find_function (struct rpl_emitter_t *instance, kan_interned_string_t name)
{
    for (uint64_t function_index = 0u; function_index < instance->intermediate->functions.size; ++function_index)
    {
        struct kan_rpl_function_t *function =
            &((struct kan_rpl_function_t *) instance->intermediate->functions.data)[function_index];

        enum conditional_evaluation_result_t condition =
            evaluate_conditional (instance, &function->conditional, KAN_FALSE);

        switch (condition)
        {
        case CONDITIONAL_EVALUATION_RESULT_TRUE:
            if (function->name == name)
            {
                return function;
            }

            break;

        case CONDITIONAL_EVALUATION_RESULT_FAILED:
        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            break;
        }
    }

    return NULL;
}

static inline kan_bool_t is_type_exists (struct rpl_emitter_t *instance, kan_interned_string_t type)
{
    return rpl_emitter_find_struct (instance, type) || find_inbuilt_vector_type (type) ||
           find_inbuilt_matrix_type (type);
}

static kan_bool_t validate_declaration (struct rpl_emitter_t *instance,
                                        struct kan_rpl_declaration_t *declaration,
                                        kan_bool_t instance_options_allowed)
{
    enum conditional_evaluation_result_t condition =
        evaluate_conditional (instance, &declaration->conditional, instance_options_allowed);
    switch (condition)
    {
    case CONDITIONAL_EVALUATION_RESULT_FAILED:
        return KAN_FALSE;

    case CONDITIONAL_EVALUATION_RESULT_TRUE:
        break;

    case CONDITIONAL_EVALUATION_RESULT_FALSE:
        return KAN_TRUE;
    }

    if (!is_type_exists (instance, declaration->type_name))
    {
        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Declaration has unknown type \"%s\".", instance->log_name,
                 declaration->source_name, (long) declaration->source_line, declaration->type_name)
        return KAN_FALSE;
    }

    kan_bool_t result = KAN_TRUE;
    for (uint64_t dimension = 0u; dimension < declaration->array_sizes.size; ++dimension)
    {
        struct kan_rpl_expression_node_t *node =
            &((struct kan_rpl_expression_node_t *) declaration->array_sizes.data)[dimension];

        struct compile_time_evaluation_value_t value =
            evaluate_compile_time_expression (instance, node, instance_options_allowed);
        switch (value.type)
        {
        case CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR:
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                     "[%s:%s] [%ld] Dimension \"%ld\" of declaration \"%s\" failed to be evaluated at compile time..",
                     instance->log_name, declaration->source_name, (long) declaration->source_line, (long) dimension,
                     declaration->name)
            result = KAN_FALSE;
            break;

        case CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN:
        case CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING:
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                     "[%s:%s] [%ld] Dimension \"%ld\" of declaration \"%s\" evaluated to non-integer value.",
                     instance->log_name, declaration->source_name, (long) declaration->source_line, (long) dimension,
                     declaration->name)
            result = KAN_FALSE;
            break;

        case CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER:
            if (value.integer_value <= 0)
            {
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                         "[%s:%s] [%ld] Dimension \"%ld\" of declaration \"%s\" evaluated to negative or zero.",
                         instance->log_name, declaration->source_name, (long) declaration->source_line,
                         (long) dimension, declaration->name)
                result = KAN_FALSE;
            }

            break;
        }
    }

    return result;
}

static kan_bool_t validate_struct (struct rpl_emitter_t *instance, struct kan_rpl_struct_t *struct_data)
{
    enum conditional_evaluation_result_t condition =
        evaluate_conditional (instance, &struct_data->conditional, KAN_TRUE);
    switch (condition)
    {
    case CONDITIONAL_EVALUATION_RESULT_FAILED:
        return KAN_FALSE;

    case CONDITIONAL_EVALUATION_RESULT_TRUE:
        break;

    case CONDITIONAL_EVALUATION_RESULT_FALSE:
        return KAN_TRUE;
    }

    kan_bool_t validation_result = KAN_TRUE;
    for (uint64_t declaration_index = 0u; declaration_index < struct_data->fields.size; ++declaration_index)
    {
        struct kan_rpl_declaration_t *declaration =
            &((struct kan_rpl_declaration_t *) struct_data->fields.data)[declaration_index];

        if (!validate_declaration (instance, declaration, KAN_FALSE))
        {
            validation_result = KAN_FALSE;
        }
    }

    return validation_result;
}

static kan_bool_t validate_declarations_for_16_alignment_compatibility (struct rpl_emitter_t *instance,
                                                                        struct kan_rpl_declaration_t *declaration)
{
    struct inbuilt_vector_type_t *vector_type = find_inbuilt_vector_type (declaration->type_name);
    if (vector_type && vector_type->items_count % 4u == 0u)
    {
        return KAN_TRUE;
    }

    struct inbuilt_matrix_type_t *matrix_type = find_inbuilt_matrix_type (declaration->type_name);
    if (matrix_type && matrix_type->rows * matrix_type->columns % 4u == 0u)
    {
        return KAN_TRUE;
    }

    struct kan_rpl_struct_t *struct_type = rpl_emitter_find_struct (instance, declaration->type_name);
    if (struct_type)
    {
        kan_bool_t valid = KAN_TRUE;
        for (uint64_t declaration_index = 0u; declaration_index < struct_type->fields.size; ++declaration_index)
        {
            struct kan_rpl_declaration_t *inner_declaration =
                &((struct kan_rpl_declaration_t *) struct_type->fields.data)[declaration_index];

            if (!validate_declarations_for_16_alignment_compatibility (instance, inner_declaration))
            {
                valid = KAN_FALSE;
            }
        }

        return valid;
    }

    KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
             "[%s:%s] [%ld] Declaration \"%s\" has type \"%s\" which size is not multiple of 16, but this kind of "
             "buffer enforces 16-alignment compatibility.",
             instance->log_name, declaration->source_name, (long) declaration->source_line, declaration->name,
             declaration->type_name)
    return KAN_FALSE;
}

static kan_bool_t validate_declarations_no_arrays (struct rpl_emitter_t *instance,
                                                   struct kan_rpl_declaration_t *declaration)
{
    if (declaration->array_sizes.size > 0u)
    {
        KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                 "[%s:%s] [%ld] Declaration \"%s\" is an array, but this kind of buffer enforces no-arrays requirement "
                 "due to memory limitations.",
                 instance->log_name, declaration->source_name, (long) declaration->source_line, declaration->name)
        return KAN_FALSE;
    }

    struct kan_rpl_struct_t *struct_type = rpl_emitter_find_struct (instance, declaration->type_name);
    if (struct_type)
    {
        kan_bool_t valid = KAN_TRUE;
        for (uint64_t declaration_index = 0u; declaration_index < struct_type->fields.size; ++declaration_index)
        {
            struct kan_rpl_declaration_t *inner_declaration =
                &((struct kan_rpl_declaration_t *) struct_type->fields.data)[declaration_index];

            if (!validate_declarations_no_arrays (instance, inner_declaration))
            {
                valid = KAN_FALSE;
            }
        }

        return valid;
    }

    return KAN_TRUE;
}

static kan_bool_t validate_buffer (struct rpl_emitter_t *instance, struct kan_rpl_buffer_t *buffer)
{
    enum conditional_evaluation_result_t condition = evaluate_conditional (instance, &buffer->conditional, KAN_FALSE);
    switch (condition)
    {
    case CONDITIONAL_EVALUATION_RESULT_FAILED:
        return KAN_FALSE;

    case CONDITIONAL_EVALUATION_RESULT_TRUE:
        break;

    case CONDITIONAL_EVALUATION_RESULT_FALSE:
        return KAN_TRUE;
    }

    kan_bool_t validation_result = KAN_TRUE;
    for (uint64_t declaration_index = 0u; declaration_index < buffer->fields.size; ++declaration_index)
    {
        struct kan_rpl_declaration_t *declaration =
            &((struct kan_rpl_declaration_t *) buffer->fields.data)[declaration_index];

        if (!validate_declaration (instance, declaration, KAN_FALSE))
        {
            validation_result = KAN_FALSE;
        }

        switch (buffer->type)
        {
        case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
            if (!validate_declarations_no_arrays (instance, declaration))
            {
                validation_result = KAN_FALSE;
            }

            break;

        case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
            break;

        case KAN_RPL_BUFFER_TYPE_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
            if (!validate_declarations_for_16_alignment_compatibility (instance, declaration))
            {
                validation_result = KAN_FALSE;
            }

            break;

        case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
            if (declaration->type_name != type_f4.name)
            {
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                         "[%s:%s] [%ld] Declaration \"%s\" has type \"%s\", but fragment outputs can only be f4's.",
                         instance->log_name, declaration->source_name, (long) declaration->source_line,
                         declaration->name, declaration->type_name)
                validation_result = KAN_FALSE;
            }

            if (declaration->array_sizes.size > 0u)
            {
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                         "[%s:%s] [%ld] Declaration \"%s\" is an array, but fragment outputs can only be f4's.",
                         instance->log_name, declaration->source_name, (long) declaration->source_line,
                         declaration->name)
                validation_result = KAN_FALSE;
            }

            break;
        }
    }

    return validation_result;
}

static kan_bool_t validate_sampler_setting (struct rpl_emitter_t *instance, struct kan_rpl_setting_t *setting)
{
    enum conditional_evaluation_result_t condition = evaluate_conditional (instance, &setting->conditional, KAN_TRUE);
    switch (condition)
    {
    case CONDITIONAL_EVALUATION_RESULT_FAILED:
        return KAN_FALSE;

    case CONDITIONAL_EVALUATION_RESULT_TRUE:
        break;

    case CONDITIONAL_EVALUATION_RESULT_FALSE:
        return KAN_TRUE;
    }

    if (setting->name == interned_mag_filter || setting->name == interned_min_filter)
    {
        if (!validate_setting_type (instance, setting, KAN_RPL_SETTING_TYPE_STRING))
        {
            return KAN_FALSE;
        }

        if (setting->string != interned_nearest && setting->string != interned_linear)
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Sampler setting \"%s\" has unsupported value \"%s\".",
                     instance->log_name, setting->source_name, (long) setting->source_line, setting->name,
                     setting->string)
            return KAN_FALSE;
        }
    }
    else if (setting->name == interned_mip_map_mode)
    {
        if (!validate_setting_type (instance, setting, KAN_RPL_SETTING_TYPE_STRING))
        {
            return KAN_FALSE;
        }

        if (setting->string != interned_nearest && setting->string != interned_linear)
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Sampler setting \"%s\" has unsupported value \"%s\".",
                     instance->log_name, setting->source_name, (long) setting->source_line, setting->name,
                     setting->string)
            return KAN_FALSE;
        }
    }
    else if (setting->name == interned_address_mode_u || setting->name == interned_address_mode_v ||
             setting->name == interned_address_mode_w)
    {
        if (!validate_setting_type (instance, setting, KAN_RPL_SETTING_TYPE_STRING))
        {
            return KAN_FALSE;
        }

        if (setting->string != interned_repeat && setting->string != interned_mirrored_repeat &&
            setting->string != interned_clamp_to_edge && setting->string != interned_clamp_to_border &&
            setting->string != interned_mirror_clamp_to_edge && setting->string != interned_mirror_clamp_to_border)
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Sampler setting \"%s\" has unsupported value \"%s\".",
                     instance->log_name, setting->source_name, (long) setting->source_line, setting->name,
                     setting->string)
            return KAN_FALSE;
        }
    }
    else
    {
        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Found unknown sampler setting \"%s\".", instance->log_name,
                 setting->source_name, (long) setting->source_line, setting->name)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static kan_bool_t validate_sampler (struct rpl_emitter_t *instance, struct kan_rpl_sampler_t *sampler)
{
    enum conditional_evaluation_result_t condition = evaluate_conditional (instance, &sampler->conditional, KAN_FALSE);
    switch (condition)
    {
    case CONDITIONAL_EVALUATION_RESULT_FAILED:
        return KAN_FALSE;

    case CONDITIONAL_EVALUATION_RESULT_TRUE:
        break;

    case CONDITIONAL_EVALUATION_RESULT_FALSE:
        return KAN_TRUE;
    }

    kan_bool_t validation_result = KAN_TRUE;
    for (uint64_t declaration_index = 0u; declaration_index < sampler->settings.size; ++declaration_index)
    {
        struct kan_rpl_setting_t *setting = &((struct kan_rpl_setting_t *) sampler->settings.data)[declaration_index];
        if (!validate_sampler_setting (instance, setting))
        {
            validation_result = KAN_FALSE;
        }
    }

    return validation_result;
}

static kan_bool_t validation_is_name_available (struct rpl_emitter_t *instance,
                                                struct validation_scope_t *scope,
                                                kan_interned_string_t name)
{
    if (!scope)
    {
        return !rpl_emitter_find_option (instance, name) && !rpl_emitter_find_buffer (instance, name) &&
               !rpl_emitter_find_sampler (instance, name) && !rpl_emitter_find_function (instance, name) &&
               !is_type_exists (instance, name);
    }

    struct validation_variable_t *variable = scope->first_variable;
    while (variable)
    {
        if (variable->name == name)
        {
            return KAN_FALSE;
        }

        variable = variable->next;
    }

    return validation_is_name_available (instance, scope->parent_scope, name);
}

static void validation_scope_add_variable (struct validation_scope_t *scope,
                                           kan_interned_string_t name,
                                           kan_interned_string_t type,
                                           struct kan_dynamic_array_t *array_sizes)
{
    struct validation_variable_t *variable =
        kan_allocate_batched (rpl_emitter_validation_allocation_group, sizeof (struct validation_variable_t));
    variable->next = scope->first_variable;
    scope->first_variable = variable;
    variable->name = name;
    variable->type.type = type;
    variable->type.array_sizes = array_sizes && array_sizes->size > 0u ? array_sizes : NULL;
    variable->type.array_sizes_offset = 0u;
}

static void validation_scope_clean_variables (struct validation_scope_t *scope)
{
    struct validation_variable_t *variable = scope->first_variable;
    scope->first_variable = NULL;

    while (variable)
    {
        struct validation_variable_t *next = variable->next;
        kan_free_batched (rpl_emitter_validation_allocation_group, variable);
        variable = next;
    }
}

static struct validation_variable_t *validation_find_variable_in_scope (struct validation_scope_t *scope,
                                                                        kan_interned_string_t name)
{
    if (!scope)
    {
        return NULL;
    }

    struct validation_variable_t *variable = scope->first_variable;
    while (variable)
    {
        if (variable->name == name)
        {
            return variable;
        }

        variable = variable->next;
    }

    return validation_find_variable_in_scope (scope->parent_scope, name);
}

static kan_bool_t validation_resolve_identifier_as_data (struct rpl_emitter_t *instance,
                                                         struct validation_scope_t *scope,
                                                         struct kan_rpl_expression_node_t *expression,
                                                         struct validation_type_info_t *type_output)
{
    kan_interned_string_t identifier = expression->identifier;
    struct validation_variable_t *variable;
    struct kan_rpl_buffer_t *buffer;
    struct kan_rpl_option_t *option;

    if ((buffer = rpl_emitter_find_buffer (instance, identifier)))
    {
        type_output->type = identifier;
        type_output->array_sizes = NULL;
        type_output->array_sizes_offset = 0u;
        return KAN_TRUE;
    }
    else if ((option = rpl_emitter_find_option (instance, identifier)))
    {
        switch (option->type)
        {
        case KAN_RPL_OPTION_TYPE_FLAG:
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                     "[%s:%s] [%ld] Flag option \"%s\" is used as operand, use conditionals instead.",
                     instance->log_name, expression->source_name, (long) expression->source_line, identifier)
            return KAN_FALSE;

        case KAN_RPL_OPTION_TYPE_COUNT:
            type_output->type = type_i1.name;
            type_output->array_sizes = NULL;
            type_output->array_sizes_offset = 0u;
            return KAN_TRUE;
        }

        KAN_ASSERT (KAN_FALSE)
        return KAN_FALSE;
    }
    else if ((variable = validation_find_variable_in_scope (scope, identifier)))
    {
        type_output->type = variable->type.type;
        type_output->array_sizes = variable->type.array_sizes;
        type_output->array_sizes_offset = 0u;
        return KAN_TRUE;
    }
    else
    {
        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Unable to resolve identifier \"%s\".", instance->log_name,
                 expression->source_name, (long) expression->source_line, identifier)
        return KAN_FALSE;
    }
}

static kan_bool_t validate_expression (struct rpl_emitter_t *instance,
                                       struct validation_scope_t *scope,
                                       struct kan_rpl_expression_node_t *expression,
                                       struct validation_type_info_t *type_output,
                                       kan_bool_t resolve_identifier);

static kan_bool_t validate_binary_operation (struct rpl_emitter_t *instance,
                                             struct validation_scope_t *scope,
                                             struct kan_rpl_expression_node_t *expression,
                                             struct validation_type_info_t *type_output)
{
    struct kan_rpl_expression_node_t *left_expression =
        &((struct kan_rpl_expression_node_t *) expression->children.data)[0u];
    struct kan_rpl_expression_node_t *right_expression =
        &((struct kan_rpl_expression_node_t *) expression->children.data)[1u];

    kan_bool_t valid = KAN_TRUE;
    struct validation_type_info_t left_operand_type;
    valid &= validate_expression (instance, scope, left_expression, &left_operand_type, KAN_TRUE);

    struct validation_type_info_t right_operand_type;
    valid &= validate_expression (instance, scope, right_expression, &right_operand_type,
                                  expression->binary_operation != KAN_RPL_BINARY_OPERATION_FIELD_ACCESS);

    if (!valid)
    {
        return KAN_FALSE;
    }

    if ((left_operand_type.array_sizes || right_operand_type.array_sizes) &&
        expression->binary_operation != KAN_RPL_BINARY_OPERATION_ARRAY_ACCESS)
    {
        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Caught attempt to execute binary operation on arrays.",
                 instance->log_name, expression->source_name, (long) expression->source_line)
        return KAN_FALSE;
    }

    switch (expression->binary_operation)
    {
    case KAN_RPL_BINARY_OPERATION_FIELD_ACCESS:
    {
        // Assert that emitter built correct tree.
        KAN_ASSERT (right_expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER)

        struct kan_rpl_struct_t *struct_data;
        struct kan_rpl_buffer_t *buffer;

        if (left_operand_type.type)
        {
            kan_interned_string_t field = right_expression->identifier;
            if ((struct_data = rpl_emitter_find_struct (instance, left_operand_type.type)))
            {
                for (uint64_t declaration_index = 0u; declaration_index < struct_data->fields.size; ++declaration_index)
                {
                    struct kan_rpl_declaration_t *declaration =
                        &((struct kan_rpl_declaration_t *) struct_data->fields.data)[declaration_index];

                    if (declaration->name == field)
                    {
                        type_output->type = declaration->type_name;
                        type_output->array_sizes =
                            declaration->array_sizes.size > 0u ? &declaration->array_sizes : NULL;
                        type_output->array_sizes_offset = 0u;
                        return KAN_TRUE;
                    }
                }

                KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] There is no field \"%s\" in type \"%s\".",
                         instance->log_name, expression->source_name, (long) expression->source_line, field,
                         left_operand_type.type)
                return KAN_FALSE;
            }

            if ((buffer = rpl_emitter_find_buffer (instance, left_operand_type.type)))
            {
                for (uint64_t declaration_index = 0u; declaration_index < buffer->fields.size; ++declaration_index)
                {
                    struct kan_rpl_declaration_t *declaration =
                        &((struct kan_rpl_declaration_t *) buffer->fields.data)[declaration_index];

                    if (declaration->name == field)
                    {
                        type_output->type = declaration->type_name;
                        type_output->array_sizes =
                            declaration->array_sizes.size > 0u ? &declaration->array_sizes : NULL;
                        type_output->array_sizes_offset = 0u;
                        return KAN_TRUE;
                    }
                }

                KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] There is no field \"%s\" in buffer \"%s\".",
                         instance->log_name, expression->source_name, (long) expression->source_line, field,
                         left_operand_type.type)
                return KAN_FALSE;
            }

            struct inbuilt_vector_type_t *vector_type;
            if ((vector_type = find_inbuilt_vector_type (left_operand_type.type)))
            {
                const uint64_t max_dimensions = sizeof (floating_vector_types) / sizeof (floating_vector_types[0u]);
                uint64_t dimensions_count = 0u;
                const char *pointer = field;

                while (KAN_TRUE)
                {
                    if (!*pointer)
                    {
                        switch (vector_type->item)
                        {
                        case INBUILT_TYPE_ITEM_FLOAT:
                            type_output->type = floating_vector_types[dimensions_count - 1u]->name;
                            break;

                        case INBUILT_TYPE_ITEM_INTEGER:
                            type_output->type = integer_vector_types[dimensions_count - 1u]->name;
                            break;
                        }

                        type_output->array_sizes = NULL;
                        return KAN_TRUE;
                    }

                    if (find_inbuilt_element_index_by_identifier_character (vector_type->items_count, *pointer) !=
                        UINT64_MAX)
                    {
                        ++dimensions_count;
                        ++pointer;
                    }
                    else
                    {
                        // Not a swizzle.
                        break;
                    }

                    if (dimensions_count > max_dimensions)
                    {
                        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Too many symbols in a swizzle \"%s\".",
                                 instance->log_name, expression->source_name, (long) expression->source_line, field)
                        return KAN_FALSE;
                    }
                }
            }

            struct inbuilt_matrix_type_t *matrix_type;
            if ((matrix_type = find_inbuilt_matrix_type (left_operand_type.type)))
            {
                if (field[0u] == 'c' && field[1u] == 'o' && field[2u] == 'l' && field[3u] >= '0' && field[3u] <= '9' &&
                    field[4u] == '\0')
                {
                    const uint64_t column_index = field[3u] - '0';
                    if (column_index < matrix_type->columns)
                    {
                        switch (matrix_type->item)
                        {
                        case INBUILT_TYPE_ITEM_FLOAT:
                            type_output->type = floating_vector_types[matrix_type->rows - 1u]->name;
                            break;

                        case INBUILT_TYPE_ITEM_INTEGER:
                            type_output->type = integer_vector_types[matrix_type->rows - 1u]->name;
                            break;
                        }

                        type_output->array_sizes = NULL;
                        return KAN_TRUE;
                    }
                }
            }
        }

        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Unable to get field \"%s\" from given expression.",
                 instance->log_name, expression->source_name, (long) expression->source_line,
                 right_expression->identifier)
        return KAN_FALSE;
    }

    case KAN_RPL_BINARY_OPERATION_ARRAY_ACCESS:
    {
        if (!left_operand_type.array_sizes)
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                     "[%s:%s] [%ld] Found attempt to access element by index of non-array type.", instance->log_name,
                     expression->source_name, (long) expression->source_line)
            return KAN_FALSE;
        }

        if (right_operand_type.type != type_i1.name)
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                     "[%s:%s] [%ld] Found attempt to access element by index using index that is not i1.",
                     instance->log_name, expression->source_name, (long) expression->source_line)
            return KAN_FALSE;
        }

        type_output->type = left_operand_type.type;
        type_output->array_sizes = left_operand_type.array_sizes;
        type_output->array_sizes_offset = left_operand_type.array_sizes_offset + 1u;

        if (type_output->array_sizes_offset >= type_output->array_sizes->size)
        {
            type_output->array_sizes = NULL;
            type_output->array_sizes_offset = 0u;
        }

        return KAN_TRUE;
    }

    case KAN_RPL_BINARY_OPERATION_ADD:
    case KAN_RPL_BINARY_OPERATION_SUBTRACT:
    {
        if (left_operand_type.type == right_operand_type.type)
        {
            type_output->type = left_operand_type.type;
            type_output->array_sizes = NULL;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] No known add/subtract operation between \"%s\" and \"%s\".",
                 instance->log_name, expression->source_name, (long) expression->source_line, left_operand_type.type,
                 right_operand_type.type)
        return KAN_FALSE;
    }

    case KAN_RPL_BINARY_OPERATION_MULTIPLY:
    {
        if (left_operand_type.type == right_operand_type.type)
        {
            type_output->type = left_operand_type.type;
            type_output->array_sizes = NULL;
            return KAN_TRUE;
        }

        struct inbuilt_vector_type_t *left_vector_type;
        if ((left_vector_type = find_inbuilt_vector_type (left_operand_type.type)))
        {
            switch (left_vector_type->item)
            {
            case INBUILT_TYPE_ITEM_FLOAT:
                if (right_operand_type.type == type_f1.name)
                {
                    type_output->type = left_operand_type.type;
                    type_output->array_sizes = NULL;
                    return KAN_TRUE;
                }

                break;

            case INBUILT_TYPE_ITEM_INTEGER:
                if (right_operand_type.type == type_i1.name)
                {
                    type_output->type = left_operand_type.type;
                    type_output->array_sizes = NULL;
                    return KAN_TRUE;
                }

                break;
            }
        }

        struct inbuilt_matrix_type_t *left_matrix_type;
        if ((left_matrix_type = find_inbuilt_matrix_type (left_operand_type.type)))
        {
            switch (left_matrix_type->item)
            {
            case INBUILT_TYPE_ITEM_FLOAT:
            {
                if (right_operand_type.type == type_f1.name)
                {
                    type_output->type = left_operand_type.type;
                    type_output->array_sizes = NULL;
                    return KAN_TRUE;
                }

                if (right_operand_type.type == floating_vector_types[left_matrix_type->columns - 1u]->name)
                {
                    type_output->type = floating_vector_types[left_matrix_type->columns - 1u]->name;
                    type_output->array_sizes = NULL;
                    return KAN_TRUE;
                }

                break;
            }

            case INBUILT_TYPE_ITEM_INTEGER:
            {
                if (right_operand_type.type == type_i1.name)
                {
                    type_output->type = left_operand_type.type;
                    type_output->array_sizes = NULL;
                    return KAN_TRUE;
                }

                if (right_operand_type.type == integer_vector_types[left_matrix_type->columns - 1u]->name)
                {
                    type_output->type = integer_vector_types[left_matrix_type->columns - 1u]->name;
                    type_output->array_sizes = NULL;
                    return KAN_TRUE;
                }

                break;
            }
            }
        }

        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] No known multiply operation between \"%s\" and \"%s\".",
                 instance->log_name, expression->source_name, (long) expression->source_line, left_operand_type.type,
                 right_operand_type.type)
        return KAN_FALSE;
    }

    case KAN_RPL_BINARY_OPERATION_DIVIDE:
    {
        if (left_operand_type.type == right_operand_type.type)
        {
            type_output->type = left_operand_type.type;
            type_output->array_sizes = NULL;
            return KAN_TRUE;
        }

        struct inbuilt_vector_type_t *left_vector_type;
        if ((left_vector_type = find_inbuilt_vector_type (left_operand_type.type)))
        {
            switch (left_vector_type->item)
            {
            case INBUILT_TYPE_ITEM_FLOAT:
                if (right_operand_type.type == type_f1.name)
                {
                    type_output->type = left_operand_type.type;
                    type_output->array_sizes = NULL;
                    return KAN_TRUE;
                }

                break;

            case INBUILT_TYPE_ITEM_INTEGER:
                if (right_operand_type.type == type_i1.name)
                {
                    type_output->type = left_operand_type.type;
                    type_output->array_sizes = NULL;
                    return KAN_TRUE;
                }

                break;
            }
        }

        struct inbuilt_matrix_type_t *left_matrix_type;
        if ((left_matrix_type = find_inbuilt_matrix_type (left_operand_type.type)))
        {
            switch (left_matrix_type->item)
            {
            case INBUILT_TYPE_ITEM_FLOAT:
            {
                if (right_operand_type.type == type_f1.name)
                {
                    type_output->type = left_operand_type.type;
                    type_output->array_sizes = NULL;
                    return KAN_TRUE;
                }

                break;
            }

            case INBUILT_TYPE_ITEM_INTEGER:
            {
                if (right_operand_type.type == type_i1.name)
                {
                    type_output->type = left_operand_type.type;
                    type_output->array_sizes = NULL;
                    return KAN_TRUE;
                }

                break;
            }
            }
        }

        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] No known divide operation between \"%s\" and \"%s\".",
                 instance->log_name, expression->source_name, (long) expression->source_line, left_operand_type.type,
                 right_operand_type.type)
        return KAN_FALSE;
    }

    case KAN_RPL_BINARY_OPERATION_MODULUS:
    {
        if (left_operand_type.type == type_i1.name && right_operand_type.type == type_i1.name)
        {
            type_output->type = type_i1.name;
            type_output->array_sizes = NULL;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] No known modulus operation between \"%s\" and \"%s\".",
                 instance->log_name, expression->source_name, (long) expression->source_line, left_operand_type.type,
                 right_operand_type.type)
        return KAN_FALSE;
    }

    case KAN_RPL_BINARY_OPERATION_ASSIGN:
    {
        if (left_operand_type.type == right_operand_type.type)
        {
            type_output->type = left_operand_type.type;
            type_output->array_sizes = NULL;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] No known assign operation between \"%s\" and \"%s\".",
                 instance->log_name, expression->source_name, (long) expression->source_line, left_operand_type.type,
                 right_operand_type.type)
        return KAN_FALSE;
    }

    case KAN_RPL_BINARY_OPERATION_AND:
    case KAN_RPL_BINARY_OPERATION_OR:
    {
        if (left_operand_type.type == interned_bool && right_operand_type.type == interned_bool)
        {
            type_output->type = interned_bool;
            type_output->array_sizes = NULL;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] No known and/or operation between \"%s\" and \"%s\".",
                 instance->log_name, expression->source_name, (long) expression->source_line, left_operand_type.type,
                 right_operand_type.type)
        return KAN_FALSE;
    }

    case KAN_RPL_BINARY_OPERATION_EQUAL:
    case KAN_RPL_BINARY_OPERATION_NOT_EQUAL:
    {
        if (left_operand_type.type == right_operand_type.type)
        {
            type_output->type = interned_bool;
            type_output->array_sizes = NULL;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                 "[%s:%s] [%ld] No known equality check operation between \"%s\" and \"%s\".", instance->log_name,
                 expression->source_name, (long) expression->source_line, left_operand_type.type,
                 right_operand_type.type)
        return KAN_FALSE;
    }

    case KAN_RPL_BINARY_OPERATION_LESS:
    case KAN_RPL_BINARY_OPERATION_GREATER:
    case KAN_RPL_BINARY_OPERATION_LESS_OR_EQUAL:
    case KAN_RPL_BINARY_OPERATION_GREATER_OR_EQUAL:
    {
        if (left_operand_type.type == type_f1.name && right_operand_type.type == type_f1.name)
        {
            type_output->type = interned_bool;
            type_output->array_sizes = NULL;
            return KAN_TRUE;
        }

        if (left_operand_type.type == type_i1.name && right_operand_type.type == type_i1.name)
        {
            type_output->type = interned_bool;
            type_output->array_sizes = NULL;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] No known comparison operation between \"%s\" and \"%s\".",
                 instance->log_name, expression->source_name, (long) expression->source_line, left_operand_type.type,
                 right_operand_type.type)
        return KAN_FALSE;
    }

    case KAN_RPL_BINARY_OPERATION_BITWISE_AND:
    case KAN_RPL_BINARY_OPERATION_BITWISE_OR:
    case KAN_RPL_BINARY_OPERATION_BITWISE_XOR:
    {
        if (left_operand_type.type == type_i1.name && right_operand_type.type == type_i1.name)
        {
            type_output->type = interned_bool;
            type_output->array_sizes = NULL;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                 "[%s:%s] [%ld] No known bitwise and/or/xor operation between \"%s\" and \"%s\".", instance->log_name,
                 expression->source_name, (long) expression->source_line, left_operand_type.type,
                 right_operand_type.type)
        return KAN_FALSE;
    }

    case KAN_RPL_BINARY_OPERATION_BITWISE_LSHIFT:
    case KAN_RPL_BINARY_OPERATION_BITWISE_RSHIFT:
    {
        if (left_operand_type.type == type_i1.name && right_operand_type.type == type_i1.name)
        {
            type_output->type = interned_bool;
            type_output->array_sizes = NULL;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                 "[%s:%s] [%ld] No known bitwise shift operation between \"%s\" and \"%s\".", instance->log_name,
                 expression->source_name, (long) expression->source_line, left_operand_type.type,
                 right_operand_type.type)
        return KAN_FALSE;
    }
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static kan_bool_t validate_unary_operation (struct rpl_emitter_t *instance,
                                            struct validation_scope_t *scope,
                                            struct kan_rpl_expression_node_t *expression,
                                            struct validation_type_info_t *type_output)
{
    struct validation_type_info_t operand_type;
    if (!validate_expression (instance, scope, &((struct kan_rpl_expression_node_t *) expression->children.data)[0u],
                              &operand_type, KAN_TRUE))
    {
        return KAN_FALSE;
    }

    if (operand_type.array_sizes)
    {
        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Caught attempt to execute unary operation on array.",
                 instance->log_name, expression->source_name, (long) expression->source_line)
        return KAN_FALSE;
    }

    switch (expression->unary_operation)
    {
    case KAN_RPL_UNARY_OPERATION_NEGATE:
        if (find_inbuilt_vector_type (operand_type.type) || find_inbuilt_matrix_type (operand_type.type))
        {
            type_output->type = operand_type.type;
            type_output->array_sizes = operand_type.array_sizes;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] No known negate operation for \"%s\".", instance->log_name,
                 expression->source_name, (long) expression->source_line, operand_type.type)
        return KAN_FALSE;

    case KAN_RPL_UNARY_OPERATION_NOT:
        if (operand_type.type == interned_bool)
        {
            type_output->type = operand_type.type;
            type_output->array_sizes = operand_type.array_sizes;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] No known not operation for \"%s\".", instance->log_name,
                 expression->source_name, (long) expression->source_line, operand_type.type)
        return KAN_FALSE;

    case KAN_RPL_UNARY_OPERATION_BITWISE_NOT:
        if (operand_type.type == type_i1.name)
        {
            type_output->type = operand_type.type;
            type_output->array_sizes = operand_type.array_sizes;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] No known bitwise not operation for \"%s\".",
                 instance->log_name, expression->source_name, (long) expression->source_line, operand_type.type)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static struct kan_rpl_function_t *find_inbuilt_function (kan_interned_string_t name)
{
    for (uint64_t function_index = 0u; function_index < inbuilt_function_library_intermediate.functions.size;
         ++function_index)
    {
        struct kan_rpl_function_t *function =
            &((struct kan_rpl_function_t *) inbuilt_function_library_intermediate.functions.data)[function_index];

        if (function->name == name)
        {
            return function;
        }
    }

    return NULL;
}

static kan_bool_t validate_function_call (struct rpl_emitter_t *instance,
                                          struct validation_scope_t *scope,
                                          struct kan_rpl_expression_node_t *expression,
                                          struct validation_type_info_t *type_output)
{
    struct kan_rpl_function_t *function = rpl_emitter_find_function (instance, expression->function_name);
    if (!function)
    {
        function = find_inbuilt_function (expression->function_name);
    }

    if (function)
    {
        type_output->type = function->return_type_name;
        type_output->array_sizes = NULL;
        type_output->array_sizes_offset = 0u;

        kan_bool_t valid = KAN_TRUE;
        uint64_t expression_index = 0u;

        for (uint64_t declaration_index = 0u; declaration_index < function->arguments.size; ++declaration_index)
        {
            struct kan_rpl_declaration_t *declaration =
                &((struct kan_rpl_declaration_t *) function->arguments.data)[declaration_index];

            enum conditional_evaluation_result_t conditional =
                evaluate_conditional (instance, &declaration->conditional, KAN_TRUE);
            kan_bool_t early_exit = KAN_FALSE;

            switch (conditional)
            {
            case CONDITIONAL_EVALUATION_RESULT_FAILED:
                valid = KAN_FALSE;
                early_exit = KAN_TRUE;
                break;

            case CONDITIONAL_EVALUATION_RESULT_TRUE:
            {
                if (expression_index < expression->children.size)
                {
                    struct validation_type_info_t argument_type;
                    if (!validate_expression (
                            instance, scope,
                            &((struct kan_rpl_expression_node_t *) expression->children.data)[expression_index],
                            &argument_type, KAN_TRUE))
                    {
                        valid = KAN_FALSE;
                    }
                    else if (argument_type.type != declaration->type_name)
                    {
                        KAN_LOG (
                            rpl_emitter, KAN_LOG_ERROR,
                            "[%s:%s] [%ld] Function \"%s\" argument \"%s\" has type \"%s\" but \"%s\" was provided.",
                            instance->log_name, expression->source_name, (long) expression->source_line,
                            expression->function_name, declaration->name, declaration->type_name, argument_type.type)
                        valid = KAN_FALSE;
                    }
                    else if (argument_type.array_sizes || declaration->array_sizes.size > 0u)
                    {
                        const uint64_t argument_dimensions =
                            argument_type.array_sizes ?
                                argument_type.array_sizes->size - argument_type.array_sizes_offset :
                                0u;

                        const uint64_t declaration_dimensions = declaration->array_sizes.size;
                        if (argument_dimensions != declaration_dimensions)
                        {
                            KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                                     "[%s:%s] [%ld] Function \"%s\" argument \"%s\"is an %lu-dimensional array while "
                                     "%lu-dimensional array is provided.",
                                     instance->log_name, expression->source_name, (long) expression->source_line,
                                     expression->function_name, declaration->name,
                                     (unsigned long) declaration_dimensions, (unsigned long) argument_dimensions)
                            valid = KAN_FALSE;
                        }
                        else
                        {
                            for (uint64_t dimension = 0u; dimension < argument_dimensions; ++dimension)
                            {
                                struct kan_rpl_expression_node_t *argument_size = &(
                                    (struct kan_rpl_expression_node_t *)
                                        argument_type.array_sizes->data)[dimension + argument_type.array_sizes_offset];

                                struct kan_rpl_expression_node_t *declaration_size =
                                    &((struct kan_rpl_expression_node_t *) declaration->array_sizes.data)[dimension];

                                struct compile_time_evaluation_value_t argument_size_value =
                                    evaluate_compile_time_expression (instance, argument_size, KAN_TRUE);
                                struct compile_time_evaluation_value_t declaration_size_value =
                                    evaluate_compile_time_expression (instance, declaration_size, KAN_TRUE);

                                if (argument_size_value.type != CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER ||
                                    declaration_size_value.type != CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)
                                {
                                    // Should be found and signaled during other checks.
                                    valid = KAN_FALSE;
                                }
                                else if (argument_size_value.integer_value != declaration_size_value.integer_value)
                                {
                                    KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                                             "[%s:%s] [%ld] Function \"%s\" argument \"%s\"is array has %lu elements "
                                             "at dimension %lu while "
                                             "array with %lu element at this dimension is provided.",
                                             instance->log_name, expression->source_name,
                                             (long) expression->source_line, expression->function_name,
                                             declaration->name, (unsigned long) declaration_size_value.integer_value,
                                             (unsigned long) dimension,
                                             (unsigned long) argument_size_value.integer_value)
                                    valid = KAN_FALSE;
                                }
                            }
                        }
                    }

                    ++expression_index;
                }
                else
                {
                    KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                             "[%s:%s] [%ld] Function \"%s\" has less arguments that expected.", instance->log_name,
                             expression->source_name, (long) expression->source_line, expression->function_name)

                    valid = KAN_FALSE;
                    early_exit = KAN_TRUE;
                }

                break;
            }

            case CONDITIONAL_EVALUATION_RESULT_FALSE:
                break;
            }

            if (early_exit)
            {
                break;
            }
        }

        if (expression_index < expression->children.size)
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Function \"%s\" has more arguments that expected.",
                     instance->log_name, expression->source_name, (long) expression->source_line,
                     expression->function_name)
            valid = KAN_FALSE;
        }

        return valid;
    }

    struct kan_rpl_sampler_t *sampler = rpl_emitter_find_sampler (instance, expression->function_name);
    if (sampler)
    {
        if (expression->children.size == 0u)
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Sampler call \"%s\" has no arguments.",
                     instance->log_name, expression->source_name, (long) expression->source_line,
                     expression->function_name)
            return KAN_FALSE;
        }

        if (expression->children.size > 1u)
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Sampler call \"%s\" has more than one argument.",
                     instance->log_name, expression->source_name, (long) expression->source_line,
                     expression->function_name)
            return KAN_FALSE;
        }

        struct validation_type_info_t argument_type;
        if (!validate_expression (instance, scope,
                                  &((struct kan_rpl_expression_node_t *) expression->children.data)[0u], &argument_type,
                                  KAN_TRUE))
        {
            return KAN_FALSE;
        }

        switch (sampler->type)
        {
        case KAN_RPL_SAMPLER_TYPE_2D:
        {
            if (argument_type.type != type_f2.name || argument_type.array_sizes)
            {
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                         "[%s:%s] [%ld] Sampler call \"%s\" with incorrect argument, expected single f2.",
                         instance->log_name, expression->source_name, (long) expression->source_line,
                         expression->function_name)
                return KAN_FALSE;
            }

            break;
        }
        }

        type_output->type = type_f4.name;
        type_output->array_sizes = NULL;
        return KAN_TRUE;
    }

    KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Unable to find function or sampler \"%s\".", instance->log_name,
             expression->source_name, (long) expression->source_line, expression->function_name)
    return KAN_FALSE;
}

static kan_bool_t validate_constructor (struct rpl_emitter_t *instance,
                                        struct validation_scope_t *scope,
                                        struct kan_rpl_expression_node_t *expression,
                                        struct validation_type_info_t *type_output)
{
    type_output->type = expression->constructor_type_name;
    type_output->array_sizes = NULL;
    type_output->array_sizes_offset = 0u;
    struct inbuilt_vector_type_t *vector_type = find_inbuilt_vector_type (expression->constructor_type_name);

    if (vector_type)
    {
        kan_bool_t valid = KAN_TRUE;
        uint64_t provided_items = 0u;

        for (uint64_t index = 0u; index < expression->children.size; ++index)
        {
            struct kan_rpl_expression_node_t *argument =
                &((struct kan_rpl_expression_node_t *) expression->children.data)[index];
            struct validation_type_info_t argument_type;

            if (validate_expression (instance, scope, argument, &argument_type, KAN_TRUE))
            {
                if (argument_type.array_sizes)
                {
                    KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                             "[%s:%s] [%ld] Vector type \"%s\" constructor arguments must have vector types, but array "
                             "is given as %lu argument.",
                             instance->log_name, expression->source_name, (long) expression->source_line,
                             expression->constructor_type_name, (unsigned long) index)
                    valid = KAN_FALSE;
                }
                else
                {
                    struct inbuilt_vector_type_t *argument_vector_type = find_inbuilt_vector_type (argument_type.type);
                    if (argument_vector_type)
                    {
                        // Item types are ignored as vector constructors can be used for conversion.
                        provided_items += argument_vector_type->items_count;
                    }
                    else
                    {
                        KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                                 "[%s:%s] [%ld] Vector type \"%s\" constructor arguments must be inbuilt vectors, but "
                                 "\"%s\" received as %lu argument.",
                                 instance->log_name, expression->source_name, (long) expression->source_line,
                                 expression->constructor_type_name, argument_type.type, (unsigned long) index)
                        valid = KAN_FALSE;
                    }
                }
            }
            else
            {
                valid = KAN_FALSE;
            }
        }

        if (provided_items != vector_type->items_count)
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                     "[%s:%s] [%ld] Vector type \"%s\" constructor expected %lu total input items, but received %lu.",
                     instance->log_name, expression->source_name, (long) expression->source_line,
                     expression->constructor_type_name, (unsigned long) vector_type->items_count,
                     (unsigned long) provided_items)
            valid = KAN_FALSE;
        }

        return valid;
    }

    struct inbuilt_matrix_type_t *matrix_type = find_inbuilt_matrix_type (expression->constructor_type_name);
    if (matrix_type)
    {
        if (expression->children.size != matrix_type->columns)
        {
            KAN_LOG (
                rpl_emitter, KAN_LOG_ERROR,
                "[%s:%s] [%ld] Matrix type \"%s\" constructor requires exactly %lu columns, but %lu were provided.",
                instance->log_name, expression->source_name, (long) expression->source_line,
                expression->constructor_type_name, (unsigned long) matrix_type->columns,
                (unsigned long) expression->children.size)
            return KAN_FALSE;
        }

        struct inbuilt_vector_type_t *column_type;
        switch (matrix_type->item)
        {
        case INBUILT_TYPE_ITEM_FLOAT:
            column_type = floating_vector_types[matrix_type->rows - 1u];
            break;

        case INBUILT_TYPE_ITEM_INTEGER:
            column_type = integer_vector_types[matrix_type->rows - 1u];
            break;
        }

        kan_bool_t valid = KAN_TRUE;
        for (uint64_t index = 0u; index < expression->children.size; ++index)
        {
            struct kan_rpl_expression_node_t *argument =
                &((struct kan_rpl_expression_node_t *) expression->children.data)[index];
            struct validation_type_info_t argument_type;

            if (validate_expression (instance, scope, argument, &argument_type, KAN_TRUE))
            {
                if (argument_type.array_sizes)
                {
                    KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                             "[%s:%s] [%ld] Matrix type \"%s\" constructor arguments must have type \"%s\", but array "
                             "is given as %lu argument.",
                             instance->log_name, expression->source_name, (long) expression->source_line,
                             expression->constructor_type_name, column_type->name, (unsigned long) index)
                    valid = KAN_FALSE;
                }
                else if (argument_type.type != column_type->name)
                {
                    KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                             "[%s:%s] [%ld] Matrix type \"%s\" constructor arguments must have type \"%s\", but \"%s\" "
                             "is given as %lu argument.",
                             instance->log_name, expression->source_name, (long) expression->source_line,
                             expression->constructor_type_name, column_type->name, argument_type.type,
                             (unsigned long) index)
                    valid = KAN_FALSE;
                }
            }
            else
            {
                valid = KAN_FALSE;
            }
        }

        return valid;
    }

    KAN_LOG (
        rpl_emitter, KAN_LOG_ERROR,
        "[%s:%s] [%ld] Encountered constructor of non-inbuilt type \"%s\", only inbuilt types are supported right now.",
        instance->log_name, expression->source_name, (long) expression->source_line, expression->constructor_type_name)
    return KAN_FALSE;
}

static kan_bool_t is_scope_inside_loop (struct validation_scope_t *scope)
{
    if (scope->loop_expression)
    {
        return KAN_TRUE;
    }

    if (scope->parent_scope)
    {
        return is_scope_inside_loop (scope->parent_scope);
    }

    return KAN_FALSE;
}

static kan_bool_t validate_expression (struct rpl_emitter_t *instance,
                                       struct validation_scope_t *scope,
                                       struct kan_rpl_expression_node_t *expression,
                                       struct validation_type_info_t *type_output,
                                       kan_bool_t resolve_identifier)
{
    type_output->type = NULL;
    type_output->array_sizes = NULL;

    switch (expression->type)
    {
    case KAN_RPL_EXPRESSION_NODE_TYPE_NOPE:
        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER:
        if (resolve_identifier)
        {
            return validation_resolve_identifier_as_data (instance, scope, expression, type_output);
        }
        else
        {
            // Identifiers are not validated by itself, they're validated by parent operations.
            return KAN_TRUE;
        }

    case KAN_RPL_EXPRESSION_NODE_TYPE_INTEGER_LITERAL:
        type_output->type = type_i1.name;
        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL:
        type_output->type = type_f1.name;
        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION:
    {
        type_output->type = expression->variable_declaration.type_name;
        type_output->array_sizes = expression->children.size > 0u ? &expression->children : NULL;
        type_output->array_sizes_offset = 0u;
        kan_bool_t valid = KAN_TRUE;

        if (!is_type_exists (instance, type_output->type))
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Declaration has unknown type \"%s\".",
                     instance->log_name, expression->source_name, (long) expression->source_line, type_output->type)
            valid = KAN_FALSE;
        }

        if (!validation_is_name_available (instance, scope, expression->variable_declaration.variable_name))
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Declaration name \"%s\" is not available for variable.",
                     instance->log_name, expression->source_name, (long) expression->source_line,
                     expression->variable_declaration.variable_name)
            valid = KAN_FALSE;
        }

        if (valid)
        {
            validation_scope_add_variable (scope, expression->variable_declaration.variable_name, type_output->type,
                                           type_output->array_sizes);
        }

        return valid;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION:
        return validate_binary_operation (instance, scope, expression, type_output);

    case KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION:
        return validate_unary_operation (instance, scope, expression, type_output);

    case KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE:
    {
        kan_bool_t valid = KAN_TRUE;
        struct validation_scope_t inner_scope = {
            .function = scope->function,
            .parent_scope = scope,
            .first_variable = NULL,
            .loop_expression = NULL,
        };

        for (uint64_t expression_index = 0u; expression_index < expression->children.size; ++expression_index)
        {
            struct kan_rpl_expression_node_t *child =
                &((struct kan_rpl_expression_node_t *) expression->children.data)[expression_index];
            struct validation_type_info_t inner_type_output;

            if (!validate_expression (instance, &inner_scope, child, &inner_type_output, KAN_TRUE))
            {
                valid = KAN_FALSE;
            }
        }

        validation_scope_clean_variables (&inner_scope);
        return valid;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL:
        return validate_function_call (instance, scope, expression, type_output);

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR:
        return validate_constructor (instance, scope, expression, type_output);

    case KAN_RPL_EXPRESSION_NODE_TYPE_IF:
    {
        kan_bool_t valid = KAN_TRUE;
        struct validation_type_info_t condition_type_info;

        if (validate_expression (instance, scope, &((struct kan_rpl_expression_node_t *) expression->children.data)[0u],
                                 &condition_type_info, KAN_TRUE))
        {
            if (condition_type_info.type != interned_bool || condition_type_info.array_sizes)
            {
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] If condition cannot be resolved to boolean.",
                         instance->log_name, expression->source_name, (long) expression->source_line)
                valid = KAN_FALSE;
            }
        }
        else
        {
            valid = KAN_FALSE;
        }

        if (!validate_expression (instance, scope,
                                  &((struct kan_rpl_expression_node_t *) expression->children.data)[1u],
                                  &condition_type_info, KAN_TRUE))
        {
            valid = KAN_FALSE;
        }

        if (expression->children.size == 3u)
        {
            if (!validate_expression (instance, scope,
                                      &((struct kan_rpl_expression_node_t *) expression->children.data)[2u],
                                      &condition_type_info, KAN_TRUE))
            {
                valid = KAN_FALSE;
            }
        }

        return valid;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FOR:
    {
        kan_bool_t valid = KAN_TRUE;
        struct validation_type_info_t child_type_info;
        struct validation_scope_t inner_scope = {
            .function = scope->function,
            .parent_scope = scope,
            .first_variable = NULL,
            .loop_expression = expression,
        };

        if (!validate_expression (instance, &inner_scope,
                                  &((struct kan_rpl_expression_node_t *) expression->children.data)[0u],
                                  &child_type_info, KAN_TRUE))
        {
            valid = KAN_FALSE;
        }

        if (validate_expression (instance, scope, &((struct kan_rpl_expression_node_t *) expression->children.data)[1u],
                                 &child_type_info, KAN_TRUE))
        {
            if (child_type_info.type != interned_bool || child_type_info.array_sizes)
            {
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] For condition cannot be resolved to boolean.",
                         instance->log_name, expression->source_name, (long) expression->source_line)
                valid = KAN_FALSE;
            }
        }
        else
        {
            valid = KAN_FALSE;
        }

        if (!validate_expression (instance, &inner_scope,
                                  &((struct kan_rpl_expression_node_t *) expression->children.data)[2u],
                                  &child_type_info, KAN_TRUE))
        {
            valid = KAN_FALSE;
        }

        if (!validate_expression (instance, &inner_scope,
                                  &((struct kan_rpl_expression_node_t *) expression->children.data)[3u],
                                  &child_type_info, KAN_TRUE))
        {
            valid = KAN_FALSE;
        }

        return valid;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_WHILE:
    {
        kan_bool_t valid = KAN_TRUE;
        struct validation_type_info_t condition_type_info;
        struct validation_scope_t inner_scope = {
            .function = scope->function,
            .parent_scope = scope,
            .first_variable = NULL,
            .loop_expression = expression,
        };

        if (validate_expression (instance, scope, &((struct kan_rpl_expression_node_t *) expression->children.data)[0u],
                                 &condition_type_info, KAN_TRUE))
        {
            if (condition_type_info.type != interned_bool || condition_type_info.array_sizes)
            {
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] While condition cannot be resolved to boolean.",
                         instance->log_name, expression->source_name, (long) expression->source_line)
                valid = KAN_FALSE;
            }
        }
        else
        {
            valid = KAN_FALSE;
        }

        if (!validate_expression (instance, &inner_scope,
                                  &((struct kan_rpl_expression_node_t *) expression->children.data)[1u],
                                  &condition_type_info, KAN_TRUE))
        {
            valid = KAN_FALSE;
        }

        return valid;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE:
    {
        enum conditional_evaluation_result_t conditional = evaluate_conditional (
            instance, &((struct kan_rpl_expression_node_t *) expression->children.data)[0u], KAN_TRUE);

        switch (conditional)
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            return KAN_FALSE;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
            return validate_expression (instance, scope,
                                        &((struct kan_rpl_expression_node_t *) expression->children.data)[1u],
                                        type_output, KAN_TRUE);

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            return KAN_TRUE;
        }

        KAN_ASSERT (KAN_FALSE)
        return KAN_FALSE;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS:
    {
        enum conditional_evaluation_result_t conditional = evaluate_conditional (
            instance, &((struct kan_rpl_expression_node_t *) expression->children.data)[0u], KAN_TRUE);

        switch (conditional)
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            return KAN_FALSE;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
        {
            kan_bool_t valid = KAN_TRUE;
            if (!validation_is_name_available (instance, scope, expression->alias_name))
            {
                KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Alias name \"%s\" is not available.",
                         instance->log_name, expression->source_name, (long) expression->source_line,
                         expression->alias_name)
                valid = KAN_FALSE;
            }

            struct validation_type_info_t internal_expression_type;
            if (!validate_expression (instance, scope,
                                      &((struct kan_rpl_expression_node_t *) expression->children.data)[1u],
                                      &internal_expression_type, KAN_TRUE))
            {
                valid = KAN_FALSE;
            }

            if (valid)
            {
                validation_scope_add_variable (scope, expression->alias_name, internal_expression_type.type,
                                               internal_expression_type.array_sizes);
            }

            return valid;
        }

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            return KAN_TRUE;
        }

        KAN_ASSERT (KAN_FALSE)
        return KAN_FALSE;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_BREAK:
    {
        if (!is_scope_inside_loop (scope))
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Found break that is not inside loop.",
                     instance->log_name, expression->source_name, (long) expression->source_line)
            return KAN_FALSE;
        }

        return KAN_TRUE;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONTINUE:
    {
        if (!is_scope_inside_loop (scope))
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Found continue that is not inside loop.",
                     instance->log_name, expression->source_name, (long) expression->source_line)
            return KAN_FALSE;
        }

        return KAN_TRUE;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_RETURN:
    {
        if (expression->children.size == 1u)
        {
            if (validate_expression (instance, scope,
                                     &((struct kan_rpl_expression_node_t *) expression->children.data)[0u], type_output,
                                     KAN_TRUE))
            {
                if (type_output->type != scope->function->return_type_name)
                {
                    KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                             "[%s:%s] [%ld] Found return with type \"%s\" but function return type is \"%s\".",
                             instance->log_name, expression->source_name, (long) expression->source_line,
                             type_output->type, scope->function->return_type_name)
                    return KAN_FALSE;
                }
                else if (type_output->array_sizes)
                {
                    KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                             "[%s:%s] [%ld] Found return with array type, which is not supported.", instance->log_name,
                             expression->source_name, (long) expression->source_line)
                    return KAN_FALSE;
                }

                return KAN_TRUE;
            }
            else
            {
                return KAN_FALSE;
            }
        }
        else
        {
            if (scope->function->return_type_name == interned_void)
            {
                return KAN_TRUE;
            }

            KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Found empty return in function that returns values.",
                     instance->log_name, expression->source_name, (long) expression->source_line)
            return KAN_FALSE;
        }
    }
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static kan_bool_t validate_ends_with_return (struct rpl_emitter_t *instance,
                                             struct kan_rpl_expression_node_t *expression)
{
    switch (expression->type)
    {
    case KAN_RPL_EXPRESSION_NODE_TYPE_NOPE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER:
    case KAN_RPL_EXPRESSION_NODE_TYPE_INTEGER_LITERAL:
    case KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL:
    case KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION:
    case KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION:
    case KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION:
    case KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS:
    case KAN_RPL_EXPRESSION_NODE_TYPE_BREAK:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONTINUE:
        KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                 "[%s:%s] [%ld] Expected return after this line as it seems to be the last in execution graph.",
                 instance->log_name, expression->source_name, (long) expression->source_line)
        return KAN_FALSE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE:
        if (expression->children.size == 0u)
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                     "[%s:%s] [%ld] Expected return after this line as it seems to be the last in execution graph.",
                     instance->log_name, expression->source_name, (long) expression->source_line)
            return KAN_FALSE;
        }

        return validate_ends_with_return (
            instance,
            &((struct kan_rpl_expression_node_t *) expression->children.data)[expression->children.size - 1u]);

    case KAN_RPL_EXPRESSION_NODE_TYPE_IF:
    {
        if (expression->children.size == 3u)
        {
            kan_bool_t both_valid = KAN_TRUE;
            if (!validate_ends_with_return (instance,
                                            &((struct kan_rpl_expression_node_t *) expression->children.data)[1u]))
            {
                both_valid = KAN_FALSE;
            }

            if (!validate_ends_with_return (instance,
                                            &((struct kan_rpl_expression_node_t *) expression->children.data)[2u]))
            {
                both_valid = KAN_FALSE;
            }

            return both_valid;
        }
        else
        {
            KAN_LOG (rpl_emitter, KAN_LOG_ERROR,
                     "[%s:%s] [%ld] Expected return after this \"if\" as it is the last block in execution graph.",
                     instance->log_name, expression->source_name, (long) expression->source_line)
            return KAN_FALSE;
        }
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FOR:
        return validate_ends_with_return (instance,
                                          &((struct kan_rpl_expression_node_t *) expression->children.data)[3u]);

    case KAN_RPL_EXPRESSION_NODE_TYPE_WHILE:
        return validate_ends_with_return (instance,
                                          &((struct kan_rpl_expression_node_t *) expression->children.data)[1u]);

    case KAN_RPL_EXPRESSION_NODE_TYPE_RETURN:
        return KAN_TRUE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static kan_bool_t validate_function (struct rpl_emitter_t *instance, struct kan_rpl_function_t *function)
{
    enum conditional_evaluation_result_t condition = evaluate_conditional (instance, &function->conditional, KAN_TRUE);
    switch (condition)
    {
    case CONDITIONAL_EVALUATION_RESULT_FAILED:
        return KAN_FALSE;

    case CONDITIONAL_EVALUATION_RESULT_TRUE:
        break;

    case CONDITIONAL_EVALUATION_RESULT_FALSE:
        return KAN_TRUE;
    }

    kan_bool_t validation_result = KAN_TRUE;
    if (function->return_type_name != interned_void && !is_type_exists (instance, function->return_type_name))
    {
        KAN_LOG (rpl_emitter, KAN_LOG_ERROR, "[%s:%s] [%ld] Function \"%s\" has unknown return type \"%s\".",
                 instance->log_name, function->source_name, (long) function->source_line, function->name,
                 function->return_type_name)
        validation_result = KAN_FALSE;
    }

    struct validation_scope_t scope = {
        .function = function,
        .parent_scope = NULL,
        .first_variable = NULL,
        .loop_expression = NULL,
    };

    for (uint64_t declaration_index = 0u; declaration_index < function->arguments.size; ++declaration_index)
    {
        struct kan_rpl_declaration_t *declaration =
            &((struct kan_rpl_declaration_t *) function->arguments.data)[declaration_index];

        if (validate_declaration (instance, declaration, KAN_TRUE))
        {
            validation_scope_add_variable (&scope, declaration->name, declaration->type_name,
                                           &declaration->array_sizes);
        }
        else
        {
            validation_result = KAN_FALSE;
        }
    }

    struct validation_type_info_t type_output;
    if (!validate_expression (instance, &scope, &function->body, &type_output, KAN_TRUE))
    {
        validation_result = KAN_FALSE;
    }

    if (function->return_type_name != interned_void)
    {
        if (!validate_ends_with_return (instance, &function->body))
        {
            validation_result = KAN_FALSE;
        }
    }

    validation_scope_clean_variables (&scope);
    return validation_result;
}

kan_bool_t kan_rpl_emitter_validate (kan_rpl_emitter_t emitter)
{
    struct rpl_emitter_t *instance = (struct rpl_emitter_t *) emitter;
    kan_bool_t validation_result = KAN_TRUE;

    for (uint64_t setting_index = 0u; setting_index < instance->intermediate->settings.size; ++setting_index)
    {
        struct kan_rpl_setting_t *setting =
            &((struct kan_rpl_setting_t *) instance->intermediate->settings.data)[setting_index];

        if (!validate_global_setting (instance, setting))
        {
            validation_result = KAN_FALSE;
        }
    }

    for (uint64_t struct_index = 0u; struct_index < instance->intermediate->structs.size; ++struct_index)
    {
        struct kan_rpl_struct_t *struct_data =
            &((struct kan_rpl_struct_t *) instance->intermediate->structs.data)[struct_index];

        if (!validate_struct (instance, struct_data))
        {
            validation_result = KAN_FALSE;
        }
    }

    for (uint64_t buffer_index = 0u; buffer_index < instance->intermediate->buffers.size; ++buffer_index)
    {
        struct kan_rpl_buffer_t *buffer =
            &((struct kan_rpl_buffer_t *) instance->intermediate->buffers.data)[buffer_index];

        if (!validate_buffer (instance, buffer))
        {
            validation_result = KAN_FALSE;
        }
    }

    for (uint64_t sampler_index = 0u; sampler_index < instance->intermediate->samplers.size; ++sampler_index)
    {
        struct kan_rpl_sampler_t *sampler =
            &((struct kan_rpl_sampler_t *) instance->intermediate->samplers.data)[sampler_index];

        if (!validate_sampler (instance, sampler))
        {
            validation_result = KAN_FALSE;
        }
    }

    for (uint64_t function_index = 0u; function_index < instance->intermediate->functions.size; ++function_index)
    {
        struct kan_rpl_function_t *function =
            &((struct kan_rpl_function_t *) instance->intermediate->functions.data)[function_index];

        if (!validate_function (instance, function))
        {
            validation_result = KAN_FALSE;
        }
    }

    return validation_result;
}

static void emit_meta_graphics_classic_settings (struct rpl_emitter_t *instance, struct kan_rpl_meta_t *meta_output)
{
    // We expect that everything is validated previously.
    meta_output->graphics_classic_settings = kan_rpl_graphics_classic_pipeline_settings_default ();

    for (uint64_t setting_index = 0u; setting_index < instance->intermediate->settings.size; ++setting_index)
    {
        struct kan_rpl_setting_t *setting =
            &((struct kan_rpl_setting_t *) instance->intermediate->settings.data)[setting_index];

        if (evaluate_conditional (instance, &setting->conditional, KAN_TRUE) != CONDITIONAL_EVALUATION_RESULT_TRUE)
        {
            continue;
        }

        if (setting->name == interned_polygon_mode && setting->type == KAN_RPL_SETTING_TYPE_STRING)
        {
            if (setting->string == interned_fill)
            {
                meta_output->graphics_classic_settings.polygon_mode = KAN_RPL_POLYGON_MODE_FILL;
            }
            else if (setting->string == interned_wireframe)
            {
                meta_output->graphics_classic_settings.polygon_mode = KAN_RPL_POLYGON_MODE_WIREFRAME;
            }
        }
        else if (setting->name == interned_cull_mode && setting->type == KAN_RPL_SETTING_TYPE_STRING)
        {
            if (setting->string == interned_back)
            {
                meta_output->graphics_classic_settings.cull_mode = KAN_RPL_CULL_MODE_BACK;
            }
        }
        else if (setting->name == interned_depth_test && setting->type == KAN_RPL_SETTING_TYPE_FLAG)
        {
            meta_output->graphics_classic_settings.depth_test = setting->flag;
        }
        else if (setting->name == interned_depth_write && setting->type == KAN_RPL_SETTING_TYPE_FLAG)
        {
            meta_output->graphics_classic_settings.depth_write = setting->flag;
        }
    }
}

static inline void build_buffer_meta_add_attribute (struct kan_rpl_meta_buffer_t *meta,
                                                    enum kan_rpl_meta_variable_type_t type,
                                                    uint64_t *attribute_binding_counter)
{
    struct kan_rpl_meta_attribute_t *attribute = kan_dynamic_array_add_last (&meta->attributes);
    if (!attribute)
    {
        kan_dynamic_array_set_capacity (&meta->attributes, KAN_MAX (1u, meta->attributes.size * 2u));
        attribute = kan_dynamic_array_add_last (&meta->attributes);
        KAN_ASSERT (attribute)
    }

    attribute->location = *attribute_binding_counter;
    ++*attribute_binding_counter;
    attribute->offset = meta->size;
    attribute->type = type;
}

static inline void build_buffer_meta_add_parameter (struct kan_rpl_meta_buffer_t *meta,
                                                    kan_interned_string_t name,
                                                    enum kan_rpl_meta_variable_type_t type,
                                                    uint64_t total_item_count,
                                                    struct kan_dynamic_array_t *declaration_meta)
{
    struct kan_rpl_meta_parameter_t *parameter = kan_dynamic_array_add_last (&meta->parameters);
    if (!parameter)
    {
        kan_dynamic_array_set_capacity (&meta->parameters, KAN_MAX (1u, meta->parameters.size * 2u));
        parameter = kan_dynamic_array_add_last (&meta->parameters);
        KAN_ASSERT (parameter)
    }

    kan_rpl_meta_parameter_init (parameter);
    parameter->name = name;
    parameter->type = type;
    parameter->offset = meta->size;
    parameter->total_item_count = total_item_count;

    kan_dynamic_array_set_capacity (&parameter->meta, declaration_meta->size);
    parameter->meta.size = declaration_meta->size;

    if (declaration_meta->size > 0u)
    {
        memcpy (parameter->meta.data, declaration_meta->data, sizeof (kan_interned_string_t) * declaration_meta->size);
    }
}

static void build_buffer_meta_from_declarations (struct rpl_emitter_t *instance,
                                                 struct kan_rpl_buffer_t *buffer,
                                                 struct kan_rpl_meta_buffer_t *meta,
                                                 struct kan_dynamic_array_t *declarations,
                                                 uint64_t *attribute_binding_counter)
{
    // We don't care about alignment here because:
    // - We've expect alignment for 16-bit based buffers to be already valid.
    // - All our component types for vectors and matrices (as of current version of RPL) are 4-byte aligned.
    // Also, we use meta size as offset counter for simplicity.

    for (uint64_t declaration_index = 0u; declaration_index < declarations->size; ++declaration_index)
    {
        struct kan_rpl_declaration_t *declaration =
            &((struct kan_rpl_declaration_t *) declarations->data)[declaration_index];

        if (evaluate_conditional (instance, &declaration->conditional, KAN_TRUE) != CONDITIONAL_EVALUATION_RESULT_TRUE)
        {
            continue;
        }

        uint64_t count = 1u;
        for (uint64_t dimension = 0u; dimension < declaration->array_sizes.size; ++dimension)
        {
            struct kan_rpl_expression_node_t *node =
                &((struct kan_rpl_expression_node_t *) declaration->array_sizes.data)[dimension];
            struct compile_time_evaluation_value_t value = evaluate_compile_time_expression (instance, node, KAN_FALSE);

            if (value.type == CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER)
            {
                count *= (uint64_t) value.integer_value;
            }
        }

        struct inbuilt_vector_type_t *vector_type = find_inbuilt_vector_type (declaration->type_name);
        if (vector_type)
        {
            switch (buffer->type)
            {
            case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
                build_buffer_meta_add_attribute (meta, vector_type->meta_type, attribute_binding_counter);
                break;

            case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
                build_buffer_meta_add_attribute (meta, vector_type->meta_type, attribute_binding_counter);
                build_buffer_meta_add_parameter (meta, declaration->name, vector_type->meta_type, count,
                                                 &declaration->meta);
                break;

            case KAN_RPL_BUFFER_TYPE_UNIFORM:
            case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
                build_buffer_meta_add_parameter (meta, declaration->name, vector_type->meta_type, count,
                                                 &declaration->meta);
                break;

            case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
            case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
                break;
            }

            const uint64_t field_size = vector_type->items_count * 4u;
            meta->size += field_size * count;
            continue;
        }

        struct inbuilt_matrix_type_t *matrix_type = find_inbuilt_matrix_type (declaration->type_name);
        if (matrix_type)
        {
            switch (buffer->type)
            {
            case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
                build_buffer_meta_add_attribute (meta, matrix_type->meta_type, attribute_binding_counter);
                break;

            case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
                build_buffer_meta_add_attribute (meta, matrix_type->meta_type, attribute_binding_counter);
                build_buffer_meta_add_parameter (meta, declaration->name, matrix_type->meta_type, count,
                                                 &declaration->meta);
                break;

            case KAN_RPL_BUFFER_TYPE_UNIFORM:
            case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
                build_buffer_meta_add_parameter (meta, declaration->name, matrix_type->meta_type, count,
                                                 &declaration->meta);
                break;

            case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
            case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
                break;
            }

            const uint64_t field_size = matrix_type->rows * matrix_type->columns * 4u;
            meta->size += field_size * count;
            continue;
        }

        struct kan_rpl_struct_t *struct_data = rpl_emitter_find_struct (instance, declaration->type_name);
        if (struct_data)
        {
            build_buffer_meta_from_declarations (instance, buffer, meta, &struct_data->fields,
                                                 attribute_binding_counter);
            continue;
        }

        // No more variants, but we expect data to be valid, therefore we're skipping failing here
        // as we should never be here.
    }
}

static void emit_meta_sampler_settings (struct rpl_emitter_t *instance,
                                        struct kan_rpl_sampler_t *sampler,
                                        struct kan_rpl_meta_sampler_t *meta)
{
    // We expect that everything is validated previously.
    meta->settings = kan_rpl_meta_sampler_settings_default ();

    for (uint64_t setting_index = 0u; setting_index < sampler->settings.size; ++setting_index)
    {
        struct kan_rpl_setting_t *setting = &((struct kan_rpl_setting_t *) sampler->settings.data)[setting_index];
        if (evaluate_conditional (instance, &setting->conditional, KAN_TRUE) != CONDITIONAL_EVALUATION_RESULT_TRUE)
        {
            continue;
        }

        if (setting->name == interned_mag_filter && setting->type == KAN_RPL_SETTING_TYPE_STRING)
        {
            if (setting->string == interned_nearest)
            {
                meta->settings.mag_filter = KAN_RPL_META_SAMPLER_FILTER_NEAREST;
            }
            else if (setting->string == interned_linear)
            {
                meta->settings.mag_filter = KAN_RPL_META_SAMPLER_FILTER_LINEAR;
            }
        }
        else if (setting->name == interned_min_filter && setting->type == KAN_RPL_SETTING_TYPE_STRING)
        {
            if (setting->string == interned_nearest)
            {
                meta->settings.min_filter = KAN_RPL_META_SAMPLER_FILTER_NEAREST;
            }
            else if (setting->string == interned_linear)
            {
                meta->settings.min_filter = KAN_RPL_META_SAMPLER_FILTER_LINEAR;
            }
        }
        else if (setting->name == interned_mip_map_mode && setting->type == KAN_RPL_SETTING_TYPE_STRING)
        {
            if (setting->string == interned_nearest)
            {
                meta->settings.mip_map_mode = KAN_RPL_META_SAMPLER_MIP_MAP_MODE_NEAREST;
            }
            else if (setting->string == interned_linear)
            {
                meta->settings.mip_map_mode = KAN_RPL_META_SAMPLER_MIP_MAP_MODE_LINEAR;
            }
        }
        else if (setting->name == interned_address_mode_u && setting->type == KAN_RPL_SETTING_TYPE_STRING)
        {
            if (setting->string == interned_repeat)
            {
                meta->settings.address_mode_u = KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT;
            }
            else if (setting->string == interned_mirrored_repeat)
            {
                meta->settings.address_mode_u = KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            }
            else if (setting->string == interned_clamp_to_edge)
            {
                meta->settings.address_mode_u = KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            }
            else if (setting->string == interned_clamp_to_border)
            {
                meta->settings.address_mode_u = KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            }
            else if (setting->string == interned_mirror_clamp_to_edge)
            {
                meta->settings.address_mode_u = KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
            }
            else if (setting->string == interned_mirror_clamp_to_border)
            {
                meta->settings.address_mode_u = KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_BORDER;
            }
        }
        else if (setting->name == interned_address_mode_v && setting->type == KAN_RPL_SETTING_TYPE_STRING)
        {
            if (setting->string == interned_repeat)
            {
                meta->settings.address_mode_v = KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT;
            }
            else if (setting->string == interned_mirrored_repeat)
            {
                meta->settings.address_mode_v = KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            }
            else if (setting->string == interned_clamp_to_edge)
            {
                meta->settings.address_mode_v = KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            }
            else if (setting->string == interned_clamp_to_border)
            {
                meta->settings.address_mode_v = KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            }
            else if (setting->string == interned_mirror_clamp_to_edge)
            {
                meta->settings.address_mode_v = KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
            }
            else if (setting->string == interned_mirror_clamp_to_border)
            {
                meta->settings.address_mode_v = KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_BORDER;
            }
        }
        else if (setting->name == interned_address_mode_w && setting->type == KAN_RPL_SETTING_TYPE_STRING)
        {
            if (setting->string == interned_repeat)
            {
                meta->settings.address_mode_w = KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT;
            }
            else if (setting->string == interned_mirrored_repeat)
            {
                meta->settings.address_mode_w = KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            }
            else if (setting->string == interned_clamp_to_edge)
            {
                meta->settings.address_mode_w = KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            }
            else if (setting->string == interned_clamp_to_border)
            {
                meta->settings.address_mode_w = KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            }
            else if (setting->string == interned_mirror_clamp_to_edge)
            {
                meta->settings.address_mode_w = KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
            }
            else if (setting->string == interned_mirror_clamp_to_border)
            {
                meta->settings.address_mode_w = KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_BORDER;
            }
        }
    }
}

kan_bool_t kan_rpl_emitter_emit_meta (kan_rpl_emitter_t emitter, struct kan_rpl_meta_t *meta_output)
{
    struct rpl_emitter_t *instance = (struct rpl_emitter_t *) emitter;
    meta_output->pipeline_type = instance->pipeline_type;

    switch (instance->pipeline_type)
    {
    case KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC:
        emit_meta_graphics_classic_settings (instance, meta_output);
        break;
    }

    uint64_t vertex_buffer_binding_index = 0u;
    uint64_t data_buffer_binding_index = 0u;
    uint64_t attribute_binding_index = 0u;

    for (uint64_t buffer_index = 0u; buffer_index < instance->intermediate->buffers.size; ++buffer_index)
    {
        struct kan_rpl_buffer_t *buffer =
            &((struct kan_rpl_buffer_t *) instance->intermediate->buffers.data)[buffer_index];

        if (buffer->type == KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT)
        {
            continue;
        }

        if (evaluate_conditional (instance, &buffer->conditional, KAN_TRUE) != CONDITIONAL_EVALUATION_RESULT_TRUE)
        {
            continue;
        }

        if (buffer->type == KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT)
        {
            if (instance->pipeline_type == KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC)
            {
                meta_output->graphics_classic_settings.fragment_output_count += buffer->fields.size;
            }

            continue;
        }

        struct kan_rpl_meta_buffer_t *meta = kan_dynamic_array_add_last (&meta_output->buffers);
        if (!meta)
        {
            kan_dynamic_array_set_capacity (&meta_output->buffers, KAN_MAX (1u, meta_output->buffers.size * 2u));
            meta = kan_dynamic_array_add_last (&meta_output->buffers);
            KAN_ASSERT (meta)
        }

        kan_rpl_meta_buffer_init (meta);
        meta->name = buffer->name;

        switch (buffer->type)
        {
        case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
            meta->binding = vertex_buffer_binding_index;
            ++vertex_buffer_binding_index;
            break;

        case KAN_RPL_BUFFER_TYPE_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
            meta->binding = data_buffer_binding_index;
            ++data_buffer_binding_index;
            break;

        case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
        case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
            break;
        }

        meta->type = buffer->type;
        meta->size = 0u;
        build_buffer_meta_from_declarations (instance, buffer, meta, &buffer->fields, &attribute_binding_index);
    }

    for (uint64_t sampler_index = 0u; sampler_index < instance->intermediate->samplers.size; ++sampler_index)
    {
        struct kan_rpl_sampler_t *sampler =
            &((struct kan_rpl_sampler_t *) instance->intermediate->samplers.data)[sampler_index];

        if (evaluate_conditional (instance, &sampler->conditional, KAN_TRUE) != CONDITIONAL_EVALUATION_RESULT_TRUE)
        {
            continue;
        }

        struct kan_rpl_meta_sampler_t *meta = kan_dynamic_array_add_last (&meta_output->samplers);
        if (!meta)
        {
            kan_dynamic_array_set_capacity (&meta_output->samplers, KAN_MAX (1u, meta_output->samplers.size * 2u));
            meta = kan_dynamic_array_add_last (&meta_output->samplers);
            KAN_ASSERT (meta)
        }

        meta->name = sampler->name;
        meta->binding = data_buffer_binding_index;
        ++data_buffer_binding_index;
        meta->type = sampler->type;
        emit_meta_sampler_settings (instance, sampler, meta);
    }

    kan_dynamic_array_set_capacity (&meta_output->buffers, meta_output->buffers.size);
    kan_dynamic_array_set_capacity (&meta_output->samplers, meta_output->samplers.size);
    return KAN_TRUE;
}

static inline uint32_t *spirv_new_instruction (struct spirv_generation_context_t *context,
                                               struct spirv_arbitrary_instruction_section_t *section,
                                               uint32_t word_count)
{
    struct spirv_arbitrary_instruction_item_t *item = kan_stack_group_allocator_allocate (
        &context->temporary_allocator,
        sizeof (struct spirv_arbitrary_instruction_item_t) + sizeof (uint32_t) * word_count,
        _Alignof (struct spirv_arbitrary_instruction_item_t));

    item->next = NULL;
    item->code[0u] = word_count << SpvWordCountShift;
    ;

    if (section->last)
    {
        section->last->next = item;
        section->last = item;
    }
    else
    {
        section->first = item;
        section->last = item;
    }

    context->code_word_count += word_count;
    return item->code;
}

static inline uint32_t spirv_to_word_length (uint32_t length)
{
    return (length + 1u) % sizeof (uint32_t) == 0u ? (length + 1u) / sizeof (uint32_t) :
                                                     1u + (length + 1u) / sizeof (uint32_t);
}

static inline void spirv_generate_op_name (struct spirv_generation_context_t *context,
                                           uint32_t for_id,
                                           const char *name)
{
    const uint32_t length = (uint32_t) strlen (name);
    const uint32_t word_length = spirv_to_word_length (length);
    uint32_t *code = spirv_new_instruction (context, &context->debug_section, 2u + word_length);
    code[0u] |= SpvOpCodeMask & SpvOpName;
    code[1u] = for_id;
    code[1u + word_length] = 0u;
    memcpy ((uint8_t *) (code + 2u), name, length);
}

static void spirv_generate_standard_types (struct spirv_generation_context_t *context)
{
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_VOID, "void");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_BOOLEAN, "bool");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_FLOAT, "f1");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_INTEGER, "i1");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_F2, "f2");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_F3, "f3");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_F4, "f4");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_I2, "i2");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_I3, "i3");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_I4, "i4");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_F3X3, "f3x3");
    spirv_generate_op_name (context, SPIRV_FIXED_ID_TYPE_F4X4, "f4x4");

    uint32_t *code = spirv_new_instruction (context, &context->type_section, 2u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVoid;
    code[1u] = SPIRV_FIXED_ID_TYPE_VOID;

    code = spirv_new_instruction (context, &context->type_section, 2u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeBool;
    code[1u] = SPIRV_FIXED_ID_TYPE_BOOLEAN;

    code = spirv_new_instruction (context, &context->type_section, 3u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeFloat;
    code[1u] = SPIRV_FIXED_ID_TYPE_FLOAT;
    code[2u] = 32u;

    code = spirv_new_instruction (context, &context->type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeInt;
    code[1u] = SPIRV_FIXED_ID_TYPE_INTEGER;
    code[2u] = 32u;
    code[3u] = 1u; // Signed.

    code = spirv_new_instruction (context, &context->type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_F2;
    code[2u] = SPIRV_FIXED_ID_TYPE_FLOAT;
    code[3u] = 2u;

    code = spirv_new_instruction (context, &context->type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_F3;
    code[2u] = SPIRV_FIXED_ID_TYPE_FLOAT;
    code[3u] = 3u;

    code = spirv_new_instruction (context, &context->type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_F4;
    code[2u] = SPIRV_FIXED_ID_TYPE_FLOAT;
    code[3u] = 4u;

    code = spirv_new_instruction (context, &context->type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_I2;
    code[2u] = SPIRV_FIXED_ID_TYPE_INTEGER;
    code[3u] = 2u;

    code = spirv_new_instruction (context, &context->type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_I3;
    code[2u] = SPIRV_FIXED_ID_TYPE_INTEGER;
    code[3u] = 3u;

    code = spirv_new_instruction (context, &context->type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeVector;
    code[1u] = SPIRV_FIXED_ID_TYPE_I4;
    code[2u] = SPIRV_FIXED_ID_TYPE_INTEGER;
    code[3u] = 4u;

    code = spirv_new_instruction (context, &context->type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeMatrix;
    code[1u] = SPIRV_FIXED_ID_TYPE_F3X3;
    code[2u] = SPIRV_FIXED_ID_TYPE_F3;
    code[3u] = 3u;

    code = spirv_new_instruction (context, &context->type_section, 4u);
    code[0u] |= SpvOpCodeMask & SpvOpTypeMatrix;
    code[1u] = SPIRV_FIXED_ID_TYPE_F4X4;
    code[2u] = SPIRV_FIXED_ID_TYPE_F4;
    code[3u] = 4u;
}

static void spirv_init_generation_context (struct spirv_generation_context_t *context,
                                           enum kan_rpl_pipeline_stage_t stage)
{
    context->current_bound = (uint32_t) SPIRV_FIXED_ID_END;
    context->code_word_count = 0u;
    context->emit_result = KAN_TRUE;
    context->stage = stage;

    context->first_struct_id = NULL;
    context->first_buffer_id = NULL;
    context->first_function_id = NULL;

    context->debug_section.first = NULL;
    context->debug_section.last = NULL;
    context->annotation_section.first = NULL;
    context->annotation_section.last = NULL;
    context->type_section.first = NULL;
    context->type_section.last = NULL;
    context->global_variable_section.first = NULL;
    context->global_variable_section.last = NULL;
    context->functions_section.first = NULL;
    context->functions_section.last = NULL;

    kan_stack_group_allocator_init (&context->temporary_allocator, rpl_emitter_generation_allocation_group,
                                    KAN_RPL_PARSER_SPIRV_GENERATION_TEMPORARY_SIZE);

    spirv_generate_standard_types (context);
}

static inline void spirv_copy_instructions (uint32_t **output,
                                            struct spirv_arbitrary_instruction_item_t *instruction_item)
{
    while (instruction_item)
    {
        const uint32_t word_count = (*instruction_item->code & ~SpvOpCodeMask) >> SpvWordCountShift;
        memcpy (*output, instruction_item->code, word_count * sizeof (uint32_t));
        *output += word_count;
        instruction_item = instruction_item->next;
    }
}

static kan_bool_t spirv_finalize_generation_context (struct spirv_generation_context_t *context,
                                                     struct rpl_emitter_t *instance,
                                                     kan_interned_string_t entry_function_name,
                                                     struct kan_dynamic_array_t *code_output)
{
    struct spirv_arbitrary_instruction_section_t base_section;
    base_section.first = NULL;
    base_section.last = NULL;

    switch (instance->pipeline_type)
    {
    case KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC:
    {
        uint32_t *op_shader_capability = spirv_new_instruction (context, &base_section, 2u);
        *op_shader_capability |= SpvOpCodeMask & SpvOpCapability;
        *(op_shader_capability + 1u) = SpvCapabilityShader;
        break;
    }
    }

    static const char glsl_library_padded[] = "GLSL.std.450\0\0\0";
    _Static_assert (sizeof (glsl_library_padded) % sizeof (uint32_t) == 0u, "GLSL library name is really padded.");
    uint32_t *op_glsl_import =
        spirv_new_instruction (context, &base_section, 2u + sizeof (glsl_library_padded) / sizeof (uint32_t));
    op_glsl_import[0u] |= SpvOpCodeMask & SpvOpExtInstImport;
    op_glsl_import[1u] = (uint32_t) SPIRV_FIXED_ID_GLSL_LIBRARY;
    memcpy (&op_glsl_import[2u], glsl_library_padded, sizeof (glsl_library_padded));

    switch (instance->pipeline_type)
    {
    case KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC:
    {
        uint32_t *op_memory_model = spirv_new_instruction (context, &base_section, 3u);
        op_memory_model[0u] |= SpvOpCodeMask & SpvOpMemoryModel;
        op_memory_model[1u] = SpvAddressingModelLogical;
        op_memory_model[2u] = SpvMemoryModelGLSL450;
        break;
    }
    }

    // TODO: Generate entry point.

    kan_dynamic_array_set_capacity (code_output, (uint64_t) (5u + context->code_word_count) * sizeof (uint32_t));
    code_output->size = code_output->capacity;

    uint32_t *output = (uint32_t *) code_output->data;
    output[0u] = SpvMagicNumber;
    output[1u] = SpvVersion;
    output[2u] = 0u;
    output[3u] = context->current_bound;
    output[4u] = 0u;
    output += 5u;

    spirv_copy_instructions (&output, base_section.first);
    spirv_copy_instructions (&output, context->debug_section.first);
    spirv_copy_instructions (&output, context->annotation_section.first);
    spirv_copy_instructions (&output, context->type_section.first);
    spirv_copy_instructions (&output, context->global_variable_section.first);
    spirv_copy_instructions (&output, context->functions_section.first);

    kan_stack_group_allocator_shutdown (&context->temporary_allocator);
    return context->emit_result;
}

kan_bool_t kan_rpl_emitter_emit_code_spirv (kan_rpl_emitter_t emitter,
                                            kan_interned_string_t entry_function_name,
                                            enum kan_rpl_pipeline_stage_t stage,
                                            struct kan_dynamic_array_t *code_output)
{
    struct rpl_emitter_t *instance = (struct rpl_emitter_t *) emitter;
    struct spirv_generation_context_t context;
    spirv_init_generation_context (&context, stage);

    // TODO: Scan for used functions and only generated used functions, used buffers and used structs.

    // TODO: While generating buffer variables, iteration should still be done on all buffers in order
    //       to correctly generated bindings and locations.

    // TODO: Implement.

    return spirv_finalize_generation_context (&context, instance, entry_function_name, code_output);
}

void kan_rpl_emitter_destroy (kan_rpl_emitter_t emitter)
{
    struct rpl_emitter_t *instance = (struct rpl_emitter_t *) emitter;
    kan_dynamic_array_shutdown (&instance->option_values);
    kan_free_general (rpl_emitter_allocation_group, instance, sizeof (struct rpl_emitter_t));
}
