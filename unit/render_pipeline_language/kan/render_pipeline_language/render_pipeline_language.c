#include <stddef.h>
#include <string.h>

#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/container/hash_storage.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/render_pipeline_language/render_pipeline_language.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (rpl_parser);
KAN_LOG_DEFINE_CATEGORY (rpl_emitter);

struct parser_option_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t name;
    enum kan_rpl_option_scope_t scope;
    enum kan_rpl_option_type_t type;

    union
    {
        kan_bool_t flag_default_value;
        uint64_t count_default_value;
    };
};

struct parser_expression_list_item_t
{
    struct parser_expression_list_item_t *next;
    struct parser_expression_tree_node_t *expression;
};

struct parser_declaration_data_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    struct parser_expression_list_item_t *array_size_list;
};

struct parser_expression_binary_operation_t
{
    enum kan_rpl_binary_operation_t binary_operation;
    struct parser_expression_tree_node_t *left_operand_expression;
    struct parser_expression_tree_node_t *right_operand_expression;
};

struct parser_expression_unary_operation_t
{
    enum kan_rpl_unary_operation_t unary_operation;
    struct parser_expression_tree_node_t *operand_expression;
};

struct parser_expression_function_call_t
{
    kan_interned_string_t function_name;
    struct parser_expression_list_item_t *arguments;
};

struct parser_expression_constructor_t
{
    kan_interned_string_t constructor_type_name;
    struct parser_expression_list_item_t *arguments;
};

struct parser_expression_if_t
{
    struct parser_expression_tree_node_t *condition_expression;
    struct parser_expression_tree_node_t *true_expression;
    struct parser_expression_tree_node_t *false_expression;
};

struct parser_expression_for_t
{
    struct parser_expression_tree_node_t *init_expression;
    struct parser_expression_tree_node_t *condition_expression;
    struct parser_expression_tree_node_t *step_expression;
    struct parser_expression_tree_node_t *body_expression;
};

struct parser_expression_while_t
{
    struct parser_expression_tree_node_t *condition_expression;
    struct parser_expression_tree_node_t *body_expression;
};

struct parser_expression_conditional_scope_t
{
    struct parser_expression_tree_node_t *condition_expression;
    struct parser_expression_tree_node_t *body_expression;
};

struct parser_expression_conditional_alias_t
{
    struct parser_expression_tree_node_t *condition_expression;
    kan_interned_string_t identifier;
    struct parser_expression_tree_node_t *body_expression;
};

struct parser_expression_tree_node_t
{
    struct parser_expression_tree_node_t *parent_expression;
    enum kan_rpl_expression_node_type_t type;

    union
    {
        kan_interned_string_t identifier;

        int64_t integer_literal;

        double floating_literal;

        struct parser_declaration_data_t variable_declaration;

        struct parser_expression_binary_operation_t binary_operation;

        struct parser_expression_unary_operation_t unary_operation;

        struct parser_expression_list_item_t *scope_expressions_list;

        struct parser_expression_function_call_t function_call;

        struct parser_expression_constructor_t constructor;

        struct parser_expression_if_t if_;

        struct parser_expression_for_t for_;

        struct parser_expression_while_t while_;

        struct parser_expression_conditional_scope_t conditional_scope;

        struct parser_expression_conditional_alias_t conditional_alias;

        struct parser_expression_tree_node_t *return_expression;
    };

    kan_interned_string_t source_log_name;
    uint64_t source_line;
};

struct parser_setting_data_t
{
    kan_interned_string_t name;
    enum kan_rpl_setting_type_t type;

    union
    {
        kan_bool_t flag;
        int64_t integer;
        double floating;
        kan_interned_string_t string;
    };

    struct parser_expression_tree_node_t *conditional;
    kan_interned_string_t source_log_name;
    uint64_t source_line;
};

struct parser_setting_t
{
    struct kan_hash_storage_node_t node;
    struct parser_setting_data_t setting;
};

enum parser_declaration_list_type_t
{
    PARSER_DECLARATION_LIST_TYPE_FIELDS = 0u,
    PARSER_DECLARATION_LIST_TYPE_ARGUMENTS,
};

struct parser_declaration_meta_item_t
{
    struct parser_declaration_meta_item_t *next;
    kan_interned_string_t meta;
};

struct parser_declaration_t
{
    struct parser_declaration_t *next;
    struct parser_declaration_data_t declaration;
    struct parser_declaration_meta_item_t *first_meta;
    struct parser_expression_tree_node_t *conditional;
    kan_interned_string_t source_log_name;
    uint64_t source_line;
};

struct parser_struct_sort_dependant_t
{
    struct parser_struct_sort_dependant_t *next;
    struct parser_struct_t *dependant;
};

struct parser_struct_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t name;
    struct parser_declaration_t *first_declaration;

    struct parser_expression_tree_node_t *conditional;
    kan_interned_string_t source_log_name;
    uint64_t source_line;

    uint64_t sort_dependencies_count;
    struct parser_struct_sort_dependant_t *sort_first_dependant;
    kan_bool_t sort_added;
};

struct parser_buffer_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t name;
    enum kan_rpl_buffer_type_t type;
    struct parser_declaration_t *first_declaration;

    struct parser_expression_tree_node_t *conditional;
    kan_interned_string_t source_log_name;
    uint64_t source_line;
};

struct parser_setting_list_item_t
{
    struct parser_setting_list_item_t *next;
    struct parser_setting_data_t setting;
};

struct parser_sampler_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t name;
    enum kan_rpl_sampler_type_t type;
    struct parser_setting_list_item_t *first_setting;

    struct parser_expression_tree_node_t *conditional;
    kan_interned_string_t source_log_name;
    uint64_t source_line;
};

struct parser_function_sort_dependant_t
{
    struct parser_function_sort_dependant_t *next;
    struct parser_function_t *dependant;
};

struct parser_function_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t return_type_name;
    kan_interned_string_t name;
    struct parser_declaration_t *first_argument;
    struct parser_expression_tree_node_t *body_expression;

    struct parser_expression_tree_node_t *conditional;
    kan_interned_string_t source_log_name;
    uint64_t source_line;

    uint64_t sort_dependencies_count;
    struct parser_function_sort_dependant_t *sort_first_dependant;
    kan_bool_t sort_added;
};

struct parser_processing_data_t
{
    struct kan_hash_storage_t options;
    struct kan_hash_storage_t settings;
    struct kan_hash_storage_t structs;
    struct kan_hash_storage_t buffers;
    struct kan_hash_storage_t samplers;
    struct kan_hash_storage_t functions;
};

struct rpl_parser_t
{
    kan_interned_string_t log_name;
    struct parser_processing_data_t processing_data;
};

struct dynamic_parser_state_t
{
    kan_interned_string_t source_log_name;
    struct parser_expression_tree_node_t *detached_conditional;

    const char *limit;
    const char *cursor;
    const char *marker;
    const char *token;

    size_t cursor_line;
    size_t cursor_symbol;
    size_t marker_line;
    size_t marker_symbol;

    const char *saved;
    size_t saved_line;
    size_t saved_symbol;

    /*!stags:re2c format = 'const char *@@;';*/
};

struct expression_parse_state_t
{
    kan_bool_t expecting_operand;
    struct parser_expression_tree_node_t *current_node;
};

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
static kan_allocation_group_t rpl_parser_allocation_group;
static kan_allocation_group_t rpl_parser_temporary_allocation_group;
static kan_allocation_group_t rpl_intermediate_allocation_group;
static kan_allocation_group_t rpl_meta_allocation_group;
static kan_allocation_group_t rpl_emission_allocation_group;
static kan_allocation_group_t rpl_emitter_allocation_group;
static kan_allocation_group_t rpl_emitter_validation_allocation_group;
static kan_allocation_group_t rpl_emitter_generation_allocation_group;

static uint64_t unary_operation_priority = 11u;

static uint64_t binary_operation_priority[] = {
    /* KAN_RPL_BINARY_OPERATION_FIELD_ACCESS */ 12u,
    /* KAN_RPL_BINARY_OPERATION_ARRAY_ACCESS */ 12u,
    /* KAN_RPL_BINARY_OPERATION_ADD */ 9u,
    /* KAN_RPL_BINARY_OPERATION_SUBTRACT */ 9u,
    /* KAN_RPL_BINARY_OPERATION_MULTIPLY */ 10u,
    /* KAN_RPL_BINARY_OPERATION_DIVIDE */ 10u,
    /* KAN_RPL_BINARY_OPERATION_MODULUS */ 10u,
    /* KAN_RPL_BINARY_OPERATION_ASSIGN */ 0u,
    /* KAN_RPL_BINARY_OPERATION_AND */ 2u,
    /* KAN_RPL_BINARY_OPERATION_OR */ 1u,
    /* KAN_RPL_BINARY_OPERATION_EQUAL */ 6u,
    /* KAN_RPL_BINARY_OPERATION_NOT_EQUAL */ 6u,
    /* KAN_RPL_BINARY_OPERATION_LESS */ 7u,
    /* KAN_RPL_BINARY_OPERATION_GREATER */ 7u,
    /* KAN_RPL_BINARY_OPERATION_LESS_OR_EQUAL */ 7u,
    /* KAN_RPL_BINARY_OPERATION_GREATER_OR_EQUAL */ 7u,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_AND */ 5u,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_OR */ 3u,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_XOR */ 4u,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_LSHIFT */ 8u,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_RSHIFT */ 8u,
};

enum binary_operation_direction_t
{
    BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT = 0u,
    BINARY_OPERATION_DIRECTION_RIGHT_TO_LEFT,
};

static uint64_t binary_operation_direction[] = {
    /* KAN_RPL_BINARY_OPERATION_FIELD_ACCESS */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_ARRAY_ACCESS */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_ADD */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_SUBTRACT */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_MULTIPLY */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_DIVIDE */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_MODULUS */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_ASSIGN */ BINARY_OPERATION_DIRECTION_RIGHT_TO_LEFT,
    /* KAN_RPL_BINARY_OPERATION_AND */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_OR */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_EQUAL */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_NOT_EQUAL */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_LESS */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_GREATER */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_LESS_OR_EQUAL */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_GREATER_OR_EQUAL */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_AND */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_OR */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_XOR */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_LSHIFT */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
    /* KAN_RPL_BINARY_OPERATION_BITWISE_RSHIFT */ BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT,
};

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
        rpl_parser_allocation_group = kan_allocation_group_get_child (rpl_allocation_group, "parser");
        rpl_parser_temporary_allocation_group =
            kan_allocation_group_get_child (rpl_parser_allocation_group, "temporary");
        rpl_intermediate_allocation_group = kan_allocation_group_get_child (rpl_allocation_group, "intermediate");
        rpl_meta_allocation_group = kan_allocation_group_get_child (rpl_allocation_group, "meta");
        rpl_emission_allocation_group = kan_allocation_group_get_child (rpl_allocation_group, "emission");
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

static inline struct parser_expression_tree_node_t *parser_expression_tree_node_create (
    enum kan_rpl_expression_node_type_t type, kan_interned_string_t source_log_name, uint64_t source_line)
{
    struct parser_expression_tree_node_t *node =
        kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_expression_tree_node_t));

    node->parent_expression = NULL;
    node->type = type;
    node->source_log_name = source_log_name;
    node->source_line = source_line;

    switch (type)
    {
    case KAN_RPL_EXPRESSION_NODE_TYPE_NOPE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_BREAK:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONTINUE:
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER:
        node->identifier = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_INTEGER_LITERAL:
        node->integer_literal = 0;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL:
        node->floating_literal = 0.0;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION:
        node->variable_declaration.type = NULL;
        node->variable_declaration.name = NULL;
        node->variable_declaration.array_size_list = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION:
        node->binary_operation.binary_operation = KAN_RPL_BINARY_OPERATION_ADD;
        node->binary_operation.left_operand_expression = NULL;
        node->binary_operation.right_operand_expression = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION:
        node->unary_operation.unary_operation = KAN_RPL_UNARY_OPERATION_NEGATE;
        node->unary_operation.operand_expression = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE:
        node->scope_expressions_list = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL:
        node->function_call.function_name = NULL;
        node->function_call.arguments = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR:
        node->constructor.constructor_type_name = NULL;
        node->constructor.arguments = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_IF:
        node->if_.condition_expression = NULL;
        node->if_.true_expression = NULL;
        node->if_.false_expression = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FOR:
        node->for_.init_expression = NULL;
        node->for_.condition_expression = NULL;
        node->for_.step_expression = NULL;
        node->for_.body_expression = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_WHILE:
        node->while_.condition_expression = NULL;
        node->while_.body_expression = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE:
        node->conditional_scope.condition_expression = NULL;
        node->conditional_scope.body_expression = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS:
        node->conditional_alias.condition_expression = NULL;
        node->conditional_alias.identifier = NULL;
        node->conditional_alias.body_expression = NULL;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_RETURN:
        node->return_expression = NULL;
        break;
    }

    return node;
}

static inline void parser_expression_tree_node_destroy (struct parser_expression_tree_node_t *node)
{
    if (!node)
    {
        return;
    }

#define DESTROY_LIST(NAME)                                                                                             \
    {                                                                                                                  \
        struct parser_expression_list_item_t *item = NAME;                                                             \
        while (item)                                                                                                   \
        {                                                                                                              \
            struct parser_expression_list_item_t *next = item->next;                                                   \
            parser_expression_tree_node_destroy (item->expression);                                                    \
            kan_free_batched (rpl_parser_allocation_group, item);                                                      \
            item = next;                                                                                               \
        }                                                                                                              \
    }

    switch (node->type)
    {
    case KAN_RPL_EXPRESSION_NODE_TYPE_NOPE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER:
    case KAN_RPL_EXPRESSION_NODE_TYPE_INTEGER_LITERAL:
    case KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL:
    case KAN_RPL_EXPRESSION_NODE_TYPE_BREAK:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONTINUE:
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION:
    {
        DESTROY_LIST (node->variable_declaration.array_size_list)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION:
        parser_expression_tree_node_destroy (node->binary_operation.left_operand_expression);
        parser_expression_tree_node_destroy (node->binary_operation.right_operand_expression);
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION:
        parser_expression_tree_node_destroy (node->unary_operation.operand_expression);
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE:
        DESTROY_LIST (node->scope_expressions_list)
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL:
        DESTROY_LIST (node->function_call.arguments)
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR:
        DESTROY_LIST (node->constructor.arguments)
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_IF:
        parser_expression_tree_node_destroy (node->if_.condition_expression);
        parser_expression_tree_node_destroy (node->if_.true_expression);
        parser_expression_tree_node_destroy (node->if_.false_expression);
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FOR:
        parser_expression_tree_node_destroy (node->for_.init_expression);
        parser_expression_tree_node_destroy (node->for_.condition_expression);
        parser_expression_tree_node_destroy (node->for_.step_expression);
        parser_expression_tree_node_destroy (node->for_.body_expression);
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_WHILE:
        parser_expression_tree_node_destroy (node->while_.condition_expression);
        parser_expression_tree_node_destroy (node->while_.body_expression);
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE:
        parser_expression_tree_node_destroy (node->conditional_scope.condition_expression);
        parser_expression_tree_node_destroy (node->conditional_scope.body_expression);
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS:
        parser_expression_tree_node_destroy (node->conditional_alias.condition_expression);
        parser_expression_tree_node_destroy (node->conditional_alias.body_expression);
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_RETURN:
        parser_expression_tree_node_destroy (node->return_expression);
        break;
    }

#undef DESTROY_LIST
    kan_free_batched (rpl_parser_allocation_group, node);
}

static inline struct parser_declaration_t *parser_declaration_create (kan_interned_string_t source_log_name,
                                                                      uint64_t source_line)
{
    struct parser_declaration_t *declaration =
        kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_declaration_t));
    declaration->next = NULL;
    declaration->declaration.name = NULL;
    declaration->declaration.type = NULL;
    declaration->declaration.array_size_list = NULL;
    declaration->first_meta = NULL;
    declaration->conditional = NULL;
    declaration->source_log_name = source_log_name;
    declaration->source_line = source_line;
    return declaration;
}

static inline void parser_destroy_declaration_meta (struct parser_declaration_meta_item_t *meta_item)
{
    while (meta_item)
    {
        struct parser_declaration_meta_item_t *next = meta_item->next;
        kan_free_batched (rpl_parser_allocation_group, meta_item);
        meta_item = next;
    }
}

static void parser_declaration_destroy (struct parser_declaration_t *instance)
{
    if (!instance)
    {
        return;
    }

    struct parser_expression_list_item_t *list_item = instance->declaration.array_size_list;
    while (list_item)
    {
        struct parser_expression_list_item_t *next = list_item->next;
        parser_expression_tree_node_destroy (list_item->expression);
        kan_free_batched (rpl_parser_allocation_group, list_item);
        list_item = next;
    }

    parser_destroy_declaration_meta (instance->first_meta);
    parser_expression_tree_node_destroy (instance->conditional);
    parser_declaration_destroy (instance->next);
}

static inline struct parser_struct_t *parser_struct_create (kan_interned_string_t name,
                                                            kan_interned_string_t source_log_name,
                                                            uint64_t source_line)
{
    struct parser_struct_t *instance =
        kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_struct_t));
    instance->node.hash = (uint64_t) name;
    instance->name = name;
    instance->first_declaration = NULL;
    instance->conditional = NULL;
    instance->source_log_name = source_log_name;
    instance->source_line = source_line;
    instance->sort_dependencies_count = 0u;
    instance->sort_first_dependant = NULL;
    instance->sort_added = KAN_FALSE;
    return instance;
}

static inline void parser_struct_destroy (struct parser_struct_t *instance)
{
    parser_declaration_destroy (instance->first_declaration);
    parser_expression_tree_node_destroy (instance->conditional);
}

static inline struct parser_buffer_t *parser_buffer_create (kan_interned_string_t name,
                                                            kan_interned_string_t source_log_name,
                                                            uint64_t source_line)
{
    struct parser_buffer_t *instance =
        kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_buffer_t));
    instance->node.hash = (uint64_t) name;
    instance->name = name;
    instance->first_declaration = NULL;
    instance->conditional = NULL;
    instance->source_log_name = source_log_name;
    instance->source_line = source_line;
    return instance;
}

static inline void parser_buffer_destroy (struct parser_buffer_t *instance)
{
    parser_declaration_destroy (instance->first_declaration);
    parser_expression_tree_node_destroy (instance->conditional);
}

static inline struct parser_sampler_t *parser_sampler_create (kan_interned_string_t name,
                                                              enum kan_rpl_sampler_type_t type,
                                                              kan_interned_string_t source_log_name,
                                                              uint64_t source_line)
{
    struct parser_sampler_t *instance =
        kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_sampler_t));
    instance->node.hash = (uint64_t) name;
    instance->name = name;
    instance->type = type;
    instance->first_setting = NULL;
    instance->conditional = NULL;
    instance->source_log_name = source_log_name;
    instance->source_line = source_line;
    return instance;
}

static inline void parser_sampler_destroy (struct parser_sampler_t *instance)
{
    struct parser_setting_list_item_t *item = instance->first_setting;
    while (item)
    {
        struct parser_setting_list_item_t *next = item->next;
        parser_expression_tree_node_destroy (item->setting.conditional);
        kan_free_batched (rpl_parser_allocation_group, item);
        item = next;
    }

    parser_expression_tree_node_destroy (instance->conditional);
}

static inline struct parser_function_t *parser_function_create (kan_interned_string_t name,
                                                                kan_interned_string_t return_type_name,
                                                                kan_interned_string_t source_log_name,
                                                                uint64_t source_line)
{
    struct parser_function_t *instance =
        kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_function_t));
    instance->node.hash = (uint64_t) name;
    instance->name = name;
    instance->return_type_name = return_type_name;
    instance->first_argument = NULL;
    instance->body_expression = NULL;
    instance->conditional = NULL;
    instance->source_log_name = source_log_name;
    instance->source_line = source_line;
    instance->sort_dependencies_count = 0u;
    instance->sort_first_dependant = NULL;
    instance->sort_added = KAN_FALSE;
    return instance;
}

static inline void parser_function_destroy (struct parser_function_t *instance)
{
    parser_declaration_destroy (instance->first_argument);
    parser_expression_tree_node_destroy (instance->body_expression);
    parser_expression_tree_node_destroy (instance->conditional);
}

static inline void parser_processing_data_init (struct parser_processing_data_t *instance)
{
    kan_hash_storage_init (&instance->options, rpl_parser_allocation_group, KAN_RPL_PARSER_OPTIONS_INITIAL_BUCKETS);
    kan_hash_storage_init (&instance->settings, rpl_parser_allocation_group, KAN_RPL_PARSER_SETTINGS_INITIAL_BUCKETS);
    kan_hash_storage_init (&instance->structs, rpl_parser_allocation_group, KAN_RPL_PARSER_STRUCTS_INITIAL_BUCKETS);
    kan_hash_storage_init (&instance->buffers, rpl_parser_allocation_group, KAN_RPL_PARSER_BUFFERS_INITIAL_BUCKETS);
    kan_hash_storage_init (&instance->samplers, rpl_parser_allocation_group, KAN_RPL_PARSER_SAMPLERS_INITIAL_BUCKETS);
    kan_hash_storage_init (&instance->functions, rpl_parser_allocation_group, KAN_RPL_PARSER_FUNCTIONS_INITIAL_BUCKETS);
}

static inline void parser_processing_data_shutdown (struct parser_processing_data_t *instance)
{
    struct parser_option_t *option = (struct parser_option_t *) instance->options.items.first;
    while (option)
    {
        struct parser_option_t *next = (struct parser_option_t *) option->node.list_node.next;
        kan_free_batched (rpl_parser_allocation_group, option);
        option = next;
    }

    struct parser_setting_t *setting = (struct parser_setting_t *) instance->settings.items.first;
    while (setting)
    {
        struct parser_setting_t *next = (struct parser_setting_t *) setting->node.list_node.next;
        parser_expression_tree_node_destroy (setting->setting.conditional);
        kan_free_batched (rpl_parser_allocation_group, setting);
        setting = next;
    }

    struct parser_struct_t *struct_data = (struct parser_struct_t *) instance->structs.items.first;
    while (struct_data)
    {
        struct parser_struct_t *next = (struct parser_struct_t *) struct_data->node.list_node.next;
        parser_struct_destroy (struct_data);
        struct_data = next;
    }

    struct parser_buffer_t *buffer = (struct parser_buffer_t *) instance->buffers.items.first;
    while (buffer)
    {
        struct parser_buffer_t *next = (struct parser_buffer_t *) buffer->node.list_node.next;
        parser_buffer_destroy (buffer);
        buffer = next;
    }

    struct parser_sampler_t *sampler = (struct parser_sampler_t *) instance->samplers.items.first;
    while (sampler)
    {
        struct parser_sampler_t *next = (struct parser_sampler_t *) sampler->node.list_node.next;
        parser_sampler_destroy (sampler);
        sampler = next;
    }

    struct parser_function_t *function = (struct parser_function_t *) instance->functions.items.first;
    while (function)
    {
        struct parser_function_t *next = (struct parser_function_t *) function->node.list_node.next;
        parser_function_destroy (function);
        function = next;
    }

    kan_hash_storage_shutdown (&instance->options);
    kan_hash_storage_shutdown (&instance->settings);
    kan_hash_storage_shutdown (&instance->structs);
    kan_hash_storage_shutdown (&instance->buffers);
    kan_hash_storage_shutdown (&instance->samplers);
    kan_hash_storage_shutdown (&instance->functions);
}

static int re2c_refill_buffer (struct dynamic_parser_state_t *parser)
{
    // We do not refill buffer as we accept only fully loaded shader code files.
    return 1;
}

static inline void re2c_yyskip (struct dynamic_parser_state_t *parser)
{
    if (*parser->cursor == '\n')
    {
        ++parser->cursor_line;
        parser->cursor_symbol = 0u;
    }

    ++parser->cursor;
    ++parser->cursor_symbol;
}

static inline void re2c_yybackup (struct dynamic_parser_state_t *parser)
{
    parser->marker = parser->cursor;
    parser->marker_line = parser->cursor_line;
    parser->marker_symbol = parser->cursor_symbol;
}

static inline void re2c_yyrestore (struct dynamic_parser_state_t *parser)
{
    parser->cursor = parser->marker;
    parser->cursor_line = parser->marker_line;
    parser->cursor_symbol = parser->marker_symbol;
}

static inline void re2c_save_cursor (struct dynamic_parser_state_t *parser)
{
    parser->saved = parser->cursor;
    parser->saved_line = parser->cursor_line;
    parser->saved_symbol = parser->cursor_symbol;
}

static inline void re2c_restore_saved_cursor (struct dynamic_parser_state_t *parser)
{
    parser->cursor = parser->saved;
    parser->cursor_line = parser->saved_line;
    parser->cursor_symbol = parser->saved_symbol;
}

/*!re2c
 re2c:api = custom;
 re2c:api:style = free-form;
 re2c:define:YYCTYPE  = char;
 re2c:define:YYLESSTHAN = "state->cursor >= state->limit";
 re2c:define:YYPEEK = "*state->cursor";
 re2c:define:YYSKIP = "re2c_yyskip (state);";
 re2c:define:YYBACKUP = "re2c_yybackup (state);";
 re2c:define:YYRESTORE = "re2c_yyrestore (state);";
 re2c:define:YYFILL   = "re2c_refill_buffer (state) == 0";
 re2c:define:YYSTAGP = "@@{tag} = state->cursor;";
 re2c:define:YYSTAGN = "@@{tag} = NULL;";
 re2c:define:YYSHIFTSTAG  = "@@{tag} += @@{shift};";
 re2c:eof = 0;
 re2c:tags = 1;
 re2c:tags:expression = "state->@@";

 separator = [\x20\x0c\x0a\x0d\x09\x0b];
 identifier = [A-Za-z_][A-Za-z0-9_]*;
 comment = "//" .* "\n";
 */

static inline struct parser_option_t *rpl_parser_find_option (struct rpl_parser_t *parser, kan_interned_string_t name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&parser->processing_data.options, (uint64_t) name);
    struct parser_option_t *node = (struct parser_option_t *) bucket->first;
    const struct parser_option_t *node_end = (struct parser_option_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == name)
        {
            return node;
        }

        node = (struct parser_option_t *) node->node.list_node.next;
    }

    return NULL;
}

static inline kan_bool_t ensure_option_name_unique (struct rpl_parser_t *parser,
                                                    struct dynamic_parser_state_t *state,
                                                    kan_interned_string_t name)
{
    if (rpl_parser_find_option (parser, name))
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Option \"%s\" is already defined.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol, name)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline kan_bool_t parse_main_option_flag (struct rpl_parser_t *parser,
                                                 struct dynamic_parser_state_t *state,
                                                 const char *name_begin,
                                                 const char *name_end,
                                                 enum kan_rpl_option_scope_t scope,
                                                 kan_bool_t value)
{
    if (state->detached_conditional)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered conditional before option which is not supported.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    if (!ensure_option_name_unique (parser, state, name))
    {
        return KAN_FALSE;
    }

    struct parser_option_t *node = kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_option_t));

    node->node.hash = (uint64_t) name;
    node->name = name;
    node->scope = scope;
    node->type = KAN_RPL_OPTION_TYPE_FLAG;
    node->flag_default_value = value;

    kan_hash_storage_update_bucket_count_default (&parser->processing_data.options,
                                                  KAN_RPL_PARSER_OPTIONS_INITIAL_BUCKETS);
    kan_hash_storage_add (&parser->processing_data.options, &node->node);
    return KAN_TRUE;
}

static inline uint64_t parse_unsigned_integer_value (struct rpl_parser_t *parser,
                                                     struct dynamic_parser_state_t *state,
                                                     const char *literal_begin,
                                                     const char *literal_end)
{
    uint64_t value = 0u;
    for (const char *cursor = literal_begin; cursor < literal_end; ++cursor)
    {
        const uint64_t old_value = value;
        value = value * 10u + (*cursor - '0');

        if (value < old_value)
        {
            KAN_LOG (rpl_parser, KAN_LOG_WARNING, "[%s:%s] [%ld:%ld]: Found unsigned int literal which is too big.",
                     parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
            return UINT64_MAX;
        }
    }

    return value;
}

static inline double parse_unsigned_floating_value (struct rpl_parser_t *parser,
                                                    struct dynamic_parser_state_t *state,
                                                    const char *literal_begin,
                                                    const char *literal_end)
{
    double value = 0.0;
    while (literal_begin < literal_end)
    {
        if (*literal_begin == '.')
        {
            ++literal_begin;
            break;
        }

        value = value * 10.0 + (double) (*literal_begin - '0');
        ++literal_begin;
    }

    double modifier = 0.1;
    while (literal_begin < literal_end)
    {
        value += modifier * (double) (*literal_begin - '0');
        modifier *= 0.1;
        ++literal_begin;
    }

    return value;
}

static inline kan_bool_t parse_main_option_count (struct rpl_parser_t *parser,
                                                  struct dynamic_parser_state_t *state,
                                                  const char *name_begin,
                                                  const char *name_end,
                                                  enum kan_rpl_option_scope_t scope,
                                                  const char *literal_begin,
                                                  const char *literal_end)
{
    if (state->detached_conditional)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered conditional before option which is not supported.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    if (!ensure_option_name_unique (parser, state, name))
    {
        return KAN_FALSE;
    }

    struct parser_option_t *node = kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_option_t));

    node->node.hash = (uint64_t) name;
    node->name = name;
    node->scope = scope;
    node->type = KAN_RPL_OPTION_TYPE_COUNT;
    node->count_default_value = parse_unsigned_integer_value (parser, state, literal_begin, literal_end);

    kan_hash_storage_update_bucket_count_default (&parser->processing_data.options,
                                                  KAN_RPL_PARSER_OPTIONS_INITIAL_BUCKETS);
    kan_hash_storage_add (&parser->processing_data.options, &node->node);
    return KAN_TRUE;
}

static struct parser_expression_tree_node_t *parse_expression (struct rpl_parser_t *parser,
                                                               struct dynamic_parser_state_t *state);

static kan_bool_t parse_call_arguments (struct rpl_parser_t *parser,
                                        struct dynamic_parser_state_t *state,
                                        struct parser_expression_tree_node_t *output);

static struct parser_expression_tree_node_t *parse_detached_conditional (struct rpl_parser_t *parser,
                                                                         struct dynamic_parser_state_t *state)
{
    if (state->detached_conditional)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Encountered conditional after other conditional.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return NULL;
    }

    struct parser_expression_tree_node_t *parsed_node = parse_expression (parser, state);
    if (!parsed_node)
    {
        return NULL;
    }

    if (*state->cursor != ')')
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Expected closing brace \")\" at the end of child grouped expression.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)

        parser_expression_tree_node_destroy (parsed_node);
        return NULL;
    }

    ++state->cursor;
    return parsed_node;
}

static inline kan_bool_t parse_main_setting_flag (struct rpl_parser_t *parser,
                                                  struct dynamic_parser_state_t *state,
                                                  const char *name_begin,
                                                  const char *name_end,
                                                  kan_bool_t value)
{
    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    struct parser_setting_t *node =
        kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_setting_t));

    node->node.hash = (uint64_t) name;
    node->setting.name = name;
    node->setting.type = KAN_RPL_SETTING_TYPE_FLAG;
    node->setting.flag = value;

    node->setting.conditional = state->detached_conditional;
    state->detached_conditional = NULL;

    node->setting.source_log_name = state->source_log_name;
    node->setting.source_line = state->cursor_line;

    kan_hash_storage_update_bucket_count_default (&parser->processing_data.settings,
                                                  KAN_RPL_PARSER_OPTIONS_INITIAL_BUCKETS);
    kan_hash_storage_add (&parser->processing_data.settings, &node->node);
    return KAN_TRUE;
}

static inline kan_bool_t parse_main_setting_integer (struct rpl_parser_t *parser,
                                                     struct dynamic_parser_state_t *state,
                                                     const char *name_begin,
                                                     const char *name_end,
                                                     const char *literal_begin,
                                                     const char *literal_end)
{
    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    struct parser_setting_t *node =
        kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_setting_t));

    node->node.hash = (uint64_t) name;
    node->setting.name = name;
    node->setting.type = KAN_RPL_SETTING_TYPE_INTEGER;

    const kan_bool_t negative = *literal_begin == '-';
    if (negative)
    {
        ++literal_begin;
    }

    const uint64_t unsigned_value = parse_unsigned_integer_value (parser, state, literal_begin, literal_end);
    if (unsigned_value > INT64_MAX)
    {
        KAN_LOG (rpl_parser, KAN_LOG_WARNING, "[%s:%s] [%ld:%ld]: Setting \"%s\" integer value is too big.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol, name)
        return KAN_FALSE;
    }

    node->setting.integer = (int64_t) unsigned_value;
    if (negative)
    {
        node->setting.integer = -node->setting.integer;
    }

    node->setting.conditional = state->detached_conditional;
    state->detached_conditional = NULL;

    node->setting.source_log_name = state->source_log_name;
    node->setting.source_line = state->cursor_line;

    kan_hash_storage_update_bucket_count_default (&parser->processing_data.settings,
                                                  KAN_RPL_PARSER_SETTINGS_INITIAL_BUCKETS);
    kan_hash_storage_add (&parser->processing_data.settings, &node->node);
    return KAN_TRUE;
}

static inline kan_bool_t parse_main_setting_floating (struct rpl_parser_t *parser,
                                                      struct dynamic_parser_state_t *state,
                                                      const char *name_begin,
                                                      const char *name_end,
                                                      const char *literal_begin,
                                                      const char *literal_end)
{
    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    struct parser_setting_t *node =
        kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_setting_t));

    node->node.hash = (uint64_t) name;
    node->setting.name = name;
    node->setting.type = KAN_RPL_SETTING_TYPE_FLOATING;

    const kan_bool_t negative = *literal_begin == '-';
    if (negative)
    {
        ++literal_begin;
    }

    const double unsigned_value = parse_unsigned_floating_value (parser, state, literal_begin, literal_end);
    node->setting.floating = unsigned_value;

    if (negative)
    {
        node->setting.floating = -node->setting.floating;
    }

    node->setting.conditional = state->detached_conditional;
    state->detached_conditional = NULL;

    node->setting.source_log_name = state->source_log_name;
    node->setting.source_line = state->cursor_line;

    kan_hash_storage_update_bucket_count_default (&parser->processing_data.settings,
                                                  KAN_RPL_PARSER_SETTINGS_INITIAL_BUCKETS);
    kan_hash_storage_add (&parser->processing_data.settings, &node->node);
    return KAN_TRUE;
}

static inline kan_bool_t parse_main_setting_string (struct rpl_parser_t *parser,
                                                    struct dynamic_parser_state_t *state,
                                                    const char *name_begin,
                                                    const char *name_end,
                                                    const char *literal_begin,
                                                    const char *literal_end)
{
    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    struct parser_setting_t *node =
        kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_setting_t));

    node->node.hash = (uint64_t) name;
    node->setting.name = name;
    node->setting.type = KAN_RPL_SETTING_TYPE_STRING;
    node->setting.string = kan_char_sequence_intern (literal_begin, literal_end);

    node->setting.conditional = state->detached_conditional;
    state->detached_conditional = NULL;

    node->setting.source_log_name = state->source_log_name;
    node->setting.source_line = state->cursor_line;

    kan_hash_storage_update_bucket_count_default (&parser->processing_data.settings,
                                                  KAN_RPL_PARSER_SETTINGS_INITIAL_BUCKETS);
    kan_hash_storage_add (&parser->processing_data.settings, &node->node);
    return KAN_TRUE;
}

static struct parser_declaration_t *parse_declarations (struct rpl_parser_t *parser,
                                                        struct dynamic_parser_state_t *state,
                                                        enum parser_declaration_list_type_t list_type);

static inline struct parser_struct_t *rpl_parser_find_struct (struct rpl_parser_t *parser, kan_interned_string_t name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&parser->processing_data.structs, (uint64_t) name);
    struct parser_struct_t *node = (struct parser_struct_t *) bucket->first;
    const struct parser_struct_t *node_end = (struct parser_struct_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == name)
        {
            return node;
        }

        node = (struct parser_struct_t *) node->node.list_node.next;
    }

    return NULL;
}

static inline kan_bool_t ensure_struct_name_unique (struct rpl_parser_t *parser,
                                                    struct dynamic_parser_state_t *state,
                                                    kan_interned_string_t name)
{
    if (rpl_parser_find_struct (parser, name))
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Struct \"%s\" is already defined.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol, name)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline kan_bool_t parse_main_struct (struct rpl_parser_t *parser,
                                            struct dynamic_parser_state_t *state,
                                            const char *name_begin,
                                            const char *name_end)
{
    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    if (!ensure_struct_name_unique (parser, state, name))
    {
        return KAN_FALSE;
    }

    struct parser_struct_t *new_struct = parser_struct_create (name, state->source_log_name, state->cursor_line);
    new_struct->conditional = state->detached_conditional;
    state->detached_conditional = NULL;
    new_struct->first_declaration = parse_declarations (parser, state, PARSER_DECLARATION_LIST_TYPE_FIELDS);

    if (!new_struct->first_declaration)
    {
        parser_struct_destroy (new_struct);
        return KAN_FALSE;
    }

    kan_hash_storage_update_bucket_count_default (&parser->processing_data.structs,
                                                  KAN_RPL_PARSER_STRUCTS_INITIAL_BUCKETS);
    kan_hash_storage_add (&parser->processing_data.structs, &new_struct->node);
    return KAN_TRUE;
}

static inline struct parser_buffer_t *rpl_parser_find_buffer (struct rpl_parser_t *parser, kan_interned_string_t name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&parser->processing_data.buffers, (uint64_t) name);
    struct parser_buffer_t *node = (struct parser_buffer_t *) bucket->first;
    const struct parser_buffer_t *node_end = (struct parser_buffer_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == name)
        {
            return node;
        }

        node = (struct parser_buffer_t *) node->node.list_node.next;
    }

    return NULL;
}

static inline kan_bool_t ensure_buffer_name_unique (struct rpl_parser_t *parser,
                                                    struct dynamic_parser_state_t *state,
                                                    kan_interned_string_t name)
{
    if (rpl_parser_find_buffer (parser, name))
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Buffer \"%s\" is already defined.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol, name)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline kan_bool_t parse_main_buffer (struct rpl_parser_t *parser,
                                            struct dynamic_parser_state_t *state,
                                            enum kan_rpl_buffer_type_t type,
                                            const char *name_begin,
                                            const char *name_end)
{
    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    if (!ensure_buffer_name_unique (parser, state, name))
    {
        return KAN_FALSE;
    }

    struct parser_buffer_t *new_buffer = parser_buffer_create (name, state->source_log_name, state->cursor_line);
    new_buffer->type = type;
    new_buffer->conditional = state->detached_conditional;
    state->detached_conditional = NULL;
    new_buffer->first_declaration = parse_declarations (parser, state, PARSER_DECLARATION_LIST_TYPE_FIELDS);

    if (!new_buffer->first_declaration)
    {
        parser_buffer_destroy (new_buffer);
        return KAN_FALSE;
    }

    kan_hash_storage_update_bucket_count_default (&parser->processing_data.buffers,
                                                  KAN_RPL_PARSER_STRUCTS_INITIAL_BUCKETS);
    kan_hash_storage_add (&parser->processing_data.buffers, &new_buffer->node);
    return KAN_TRUE;
}

static inline struct parser_sampler_t *rpl_parser_find_sampler (struct rpl_parser_t *parser, kan_interned_string_t name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&parser->processing_data.samplers, (uint64_t) name);
    struct parser_sampler_t *node = (struct parser_sampler_t *) bucket->first;
    const struct parser_sampler_t *node_end = (struct parser_sampler_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == name)
        {
            return node;
        }

        node = (struct parser_sampler_t *) node->node.list_node.next;
    }

    return NULL;
}

static inline struct parser_function_t *rpl_parser_find_function (struct rpl_parser_t *parser,
                                                                  kan_interned_string_t name);

static inline kan_bool_t ensure_sampler_name_unique (struct rpl_parser_t *parser,
                                                     struct dynamic_parser_state_t *state,
                                                     kan_interned_string_t name)
{
    if (rpl_parser_find_sampler (parser, name))
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Sampler \"%s\" is already defined.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol, name)
        return KAN_FALSE;
    }

    if (rpl_parser_find_function (parser, name))
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Sampler \"%s\" is occupied by function.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol, name)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static kan_bool_t parse_main_sampler (struct rpl_parser_t *parser,
                                      struct dynamic_parser_state_t *state,
                                      enum kan_rpl_sampler_type_t type,
                                      const char *name_begin,
                                      const char *name_end);

static inline struct parser_function_t *rpl_parser_find_function (struct rpl_parser_t *parser,
                                                                  kan_interned_string_t name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&parser->processing_data.functions, (uint64_t) name);
    struct parser_function_t *node = (struct parser_function_t *) bucket->first;
    const struct parser_function_t *node_end = (struct parser_function_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == name)
        {
            return node;
        }

        node = (struct parser_function_t *) node->node.list_node.next;
    }

    return NULL;
}

static inline kan_bool_t ensure_function_name_unique (struct rpl_parser_t *parser,
                                                      struct dynamic_parser_state_t *state,
                                                      kan_interned_string_t name)
{
    if (rpl_parser_find_function (parser, name))
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Function \"%s\" is already defined.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol, name)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static kan_bool_t parse_main_function (struct rpl_parser_t *parser,
                                       struct dynamic_parser_state_t *state,
                                       const char *name_begin,
                                       const char *name_end,
                                       const char *type_name_begin,
                                       const char *type_name_end);

static kan_bool_t parse_main (struct rpl_parser_t *parser, struct dynamic_parser_state_t *state)
{
    state->detached_conditional = NULL;
    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *name_begin;
        const char *name_end;
        const char *literal_begin;
        const char *literal_end;
        const char *type_name_begin;
        const char *type_name_end;

#define CHECKED(...)                                                                                                   \
    if (!(__VA_ARGS__))                                                                                                \
    {                                                                                                                  \
        parser_expression_tree_node_destroy (state->detached_conditional);                                             \
        return KAN_FALSE;                                                                                              \
    }                                                                                                                  \
    continue;

        /*!re2c
         "global" separator+ "flag" separator+ @name_begin identifier @name_end separator+ "on" separator* ";"
         {
             CHECKED (parse_main_option_flag (parser, state, name_begin, name_end, KAN_RPL_OPTION_SCOPE_GLOBAL,
                                              KAN_TRUE))
         }
         "global" separator+ "flag" separator+ @name_begin identifier @name_end separator+ "off" separator* ";"
         {
             CHECKED (parse_main_option_flag (parser, state,name_begin, name_end, KAN_RPL_OPTION_SCOPE_GLOBAL,
                                              KAN_FALSE))
         }

         "instance" separator+ "flag" separator+ @name_begin identifier @name_end separator+ "on" separator* ";"
         {
             CHECKED (parse_main_option_flag (parser, state,name_begin, name_end, KAN_RPL_OPTION_SCOPE_INSTANCE,
                                              KAN_TRUE))
         }

         "instance" separator+ "flag" separator+ @name_begin identifier @name_end separator+ "off" separator* ";"
         {
             CHECKED (parse_main_option_flag (parser, state,name_begin, name_end, KAN_RPL_OPTION_SCOPE_INSTANCE,
                                              KAN_FALSE))
         }

         "global" separator+ "count" separator+ @name_begin identifier @name_end separator+
         @literal_begin [0-9]+ @literal_end separator* ";"
         {
             CHECKED (parse_main_option_count (parser, state, name_begin, name_end, KAN_RPL_OPTION_SCOPE_GLOBAL,
                                               literal_begin, literal_end))
         }

         "instance" separator+ "count" separator+ @name_begin identifier @name_end separator+
         @literal_begin [0-9]+ @literal_end separator* ";"
         {
             CHECKED (parse_main_option_count (parser, state, name_begin, name_end, KAN_RPL_OPTION_SCOPE_INSTANCE,
                                               literal_begin, literal_end))
         }

         "conditional" separator* "("
         {
             state->detached_conditional = parse_detached_conditional (parser, state);
             if (!state->detached_conditional)
             {
                 return KAN_FALSE;
             }

             continue;
         }

         "setting" separator+ @name_begin (identifier | ".")+ @name_end separator+ "on" separator* ";"
         { CHECKED (parse_main_setting_flag (parser, state, name_begin, name_end, KAN_TRUE)) }

         "setting" separator+ @name_begin (identifier | ".")+ @name_end separator+ "off" separator* ";"
         { CHECKED (parse_main_setting_flag (parser, state, name_begin, name_end, KAN_FALSE)) }

         "setting" separator+ @name_begin (identifier | ".")+ @name_end separator+
         @literal_begin "-"? [0-9]+ @literal_end separator* ";"
         { CHECKED (parse_main_setting_integer (parser, state, name_begin, name_end, literal_begin, literal_end)) }

         "setting" separator+ @name_begin (identifier | ".")+ @name_end separator+
         @literal_begin "-"? [0-9]+ "." [0-9]+ @literal_end separator* ";"
         { CHECKED (parse_main_setting_floating (parser, state, name_begin, name_end, literal_begin, literal_end)) }

         "setting" separator+ @name_begin (identifier | ".")+ @name_end separator+
         "\"" @literal_begin ((. \ [\x22]) | "\\\"")* @literal_end "\"" separator* ";"
         { CHECKED (parse_main_setting_string (parser, state, name_begin, name_end, literal_begin, literal_end)) }

         "struct" separator+ @name_begin identifier @name_end separator* "{"
         { CHECKED (parse_main_struct (parser, state, name_begin, name_end)) }

         "vertex_attribute_buffer" separator+ @name_begin identifier @name_end separator* "{"
         { CHECKED (parse_main_buffer (parser, state, KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE, name_begin, name_end)) }

         "uniform_buffer" separator+ @name_begin identifier @name_end separator* "{"
         { CHECKED (parse_main_buffer (parser, state, KAN_RPL_BUFFER_TYPE_UNIFORM, name_begin, name_end)) }

         "read_only_storage_buffer" separator+ @name_begin identifier @name_end separator* "{"
         { CHECKED (parse_main_buffer (parser, state, KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE, name_begin, name_end)) }

         "instanced_attribute_buffer" separator+ @name_begin identifier @name_end separator* "{"
         { CHECKED (parse_main_buffer (parser, state, KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE, name_begin, name_end)) }

         "instanced_uniform_buffer" separator+ @name_begin identifier @name_end separator* "{"
         { CHECKED (parse_main_buffer (parser, state, KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM, name_begin, name_end)) }

         "instanced_read_only_storage_buffer" separator+ @name_begin identifier @name_end separator* "{"
         {
             CHECKED (parse_main_buffer (
                     parser, state, KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE, name_begin, name_end))
         }

         "vertex_stage_output" separator+ @name_begin identifier @name_end separator* "{"
         { CHECKED (parse_main_buffer (parser, state, KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT, name_begin, name_end)) }

         "fragment_stage_output" separator+ @name_begin identifier @name_end separator* "{"
         {
             CHECKED (parse_main_buffer (
                     parser, state, KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT, name_begin, name_end))
         }

         "sampler_2d" separator+ @name_begin identifier @name_end separator* "{"
         { CHECKED (parse_main_sampler (parser, state, KAN_RPL_SAMPLER_TYPE_2D, name_begin, name_end)) }

         @type_name_begin identifier @type_name_end separator+ @name_begin identifier @name_end separator* "("
         { CHECKED (parse_main_function (parser, state, name_begin, name_end, type_name_begin, type_name_end)) }

         separator+ { continue; }
         comment+ { continue; }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression at global scope.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             parser_expression_tree_node_destroy (state->detached_conditional);
             return KAN_FALSE;
         }
         $
         {
             if (state->detached_conditional)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s]: Encountered detached conditional at the end of file.",
                          parser->log_name, state->source_log_name)
                 parser_expression_tree_node_destroy (state->detached_conditional);
                 return KAN_FALSE;
             }

             return KAN_TRUE;
         }
         */

#undef CHECKED
    }
}

static kan_bool_t parse_expression_integer_literal (struct rpl_parser_t *parser,
                                                    struct dynamic_parser_state_t *state,
                                                    struct expression_parse_state_t *expression_parse_state,
                                                    const char *literal_begin,
                                                    const char *literal_end)
{
    if (!expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Encountered integer literal while expecting operation.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    node->type = KAN_RPL_EXPRESSION_NODE_TYPE_INTEGER_LITERAL;
    node->source_log_name = state->source_log_name;
    node->source_line = state->cursor_line;

    const kan_bool_t is_negative = *literal_begin == '-';
    if (is_negative)
    {
        ++literal_begin;
    }

    const uint64_t positive_literal = parse_unsigned_integer_value (parser, state, literal_begin, literal_end);
    if (positive_literal > INT64_MAX)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered integer literal that is bigger than maximum allowed %lld.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol,
                 (long long) INT64_MAX)
        return KAN_FALSE;
    }

    node->integer_literal = (int64_t) positive_literal;
    if (is_negative)
    {
        node->integer_literal = -node->integer_literal;
    }

    expression_parse_state->expecting_operand = KAN_FALSE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_floating_literal (struct rpl_parser_t *parser,
                                                     struct dynamic_parser_state_t *state,
                                                     struct expression_parse_state_t *expression_parse_state,
                                                     const char *literal_begin,
                                                     const char *literal_end)
{
    if (!expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered floating literal while expecting operation.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    node->type = KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL;
    node->source_log_name = state->source_log_name;
    node->source_line = state->cursor_line;

    const kan_bool_t is_negative = *literal_begin == '-';
    if (is_negative)
    {
        ++literal_begin;
    }

    const double positive_literal = parse_unsigned_floating_value (parser, state, literal_begin, literal_end);
    node->floating_literal = positive_literal;

    if (is_negative)
    {
        node->floating_literal = -node->floating_literal;
    }

    expression_parse_state->expecting_operand = KAN_FALSE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_operand_identifier (struct rpl_parser_t *parser,
                                                       struct dynamic_parser_state_t *state,
                                                       struct expression_parse_state_t *expression_parse_state,
                                                       const char *name_begin,
                                                       const char *name_end);

static kan_bool_t parse_expression_binary_operation (struct rpl_parser_t *parser,
                                                     struct dynamic_parser_state_t *state,
                                                     struct expression_parse_state_t *expression_parse_state,
                                                     enum kan_rpl_binary_operation_t operation);

static kan_bool_t parse_expression_field_access (struct rpl_parser_t *parser,
                                                 struct dynamic_parser_state_t *state,
                                                 struct expression_parse_state_t *expression_parse_state,
                                                 const char *name_begin,
                                                 const char *name_end)
{
    if (expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered field access operation while expecting operand.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    return parse_expression_binary_operation (parser, state, expression_parse_state,
                                              KAN_RPL_BINARY_OPERATION_FIELD_ACCESS) &&
           parse_expression_operand_identifier (parser, state, expression_parse_state, name_begin, name_end);
}

static inline void parse_expression_replace_in_parent (struct expression_parse_state_t *expression_parse_state,
                                                       struct parser_expression_tree_node_t *current_child,
                                                       struct parser_expression_tree_node_t *new_child)
{
    if (current_child->parent_expression)
    {
        if (current_child->parent_expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION)
        {
            if (current_child->parent_expression->binary_operation.left_operand_expression == current_child)
            {
                current_child->parent_expression->binary_operation.left_operand_expression = new_child;
            }
            else
            {
                current_child->parent_expression->binary_operation.right_operand_expression = new_child;
            }
        }
        else if (current_child->parent_expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION)
        {
            current_child->parent_expression->unary_operation.operand_expression = new_child;
        }
        else
        {
            // Other parents are not expected for this operation during expression parse.
            KAN_ASSERT (KAN_FALSE)
        }
    }

    if (expression_parse_state->current_node == current_child)
    {
        expression_parse_state->current_node = new_child;
    }
}

static kan_bool_t parse_expression_array_access (struct rpl_parser_t *parser,
                                                 struct dynamic_parser_state_t *state,
                                                 struct expression_parse_state_t *expression_parse_state)
{
    if (expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered field access operation while expecting operand.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    if (!parse_expression_binary_operation (parser, state, expression_parse_state,
                                            KAN_RPL_BINARY_OPERATION_ARRAY_ACCESS))
    {
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    struct parser_expression_tree_node_t *parsed_node = parse_expression (parser, state);

    if (!parsed_node)
    {
        return KAN_FALSE;
    }

    parsed_node->parent_expression = node->parent_expression;
    parse_expression_replace_in_parent (expression_parse_state, node, parsed_node);
    parser_expression_tree_node_destroy (node);

    if (*state->cursor != ']')
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Expected closing brace \"]\" at the end of array index expression.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    ++state->cursor;
    expression_parse_state->expecting_operand = KAN_FALSE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_operand_constructor (struct rpl_parser_t *parser,
                                                        struct dynamic_parser_state_t *state,
                                                        struct expression_parse_state_t *expression_parse_state,
                                                        const char *name_begin,
                                                        const char *name_end)
{
    if (!expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered constructor operand while expecting operation.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    node->type = KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR;
    node->source_log_name = state->source_log_name;
    node->source_line = state->cursor_line;
    node->constructor.constructor_type_name = kan_char_sequence_intern (name_begin, name_end);
    node->constructor.arguments = NULL;

    if (!parse_call_arguments (parser, state, node))
    {
        return KAN_FALSE;
    }

    if (*state->cursor != '}')
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Expected closing brace \"}\" at the end of constructor.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    ++state->cursor;
    expression_parse_state->expecting_operand = KAN_FALSE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_operand_function_call (struct rpl_parser_t *parser,
                                                          struct dynamic_parser_state_t *state,
                                                          struct expression_parse_state_t *expression_parse_state,
                                                          const char *name_begin,
                                                          const char *name_end)
{
    if (!expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered function call operand while expecting operation.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    node->type = KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL;
    node->source_log_name = state->source_log_name;
    node->source_line = state->cursor_line;
    node->function_call.function_name = kan_char_sequence_intern (name_begin, name_end);
    node->function_call.arguments = NULL;

    if (!parse_call_arguments (parser, state, node))
    {
        return KAN_FALSE;
    }

    if (*state->cursor != ')')
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Expected closing brace \")\" at the end of function call.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    ++state->cursor;
    expression_parse_state->expecting_operand = KAN_FALSE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_operand_identifier (struct rpl_parser_t *parser,
                                                       struct dynamic_parser_state_t *state,
                                                       struct expression_parse_state_t *expression_parse_state,
                                                       const char *name_begin,
                                                       const char *name_end)
{
    if (!expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Encountered identifier while expecting operation.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    node->type = KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER;
    node->source_log_name = state->source_log_name;
    node->source_line = state->cursor_line;
    node->identifier = kan_char_sequence_intern (name_begin, name_end);
    expression_parse_state->expecting_operand = KAN_FALSE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_operand_child_grouped_expression (
    struct rpl_parser_t *parser,
    struct dynamic_parser_state_t *state,
    struct expression_parse_state_t *expression_parse_state)
{
    if (!expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered child grouped expression while expecting operation.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    struct parser_expression_tree_node_t *parsed_node = parse_expression (parser, state);

    if (!parsed_node)
    {
        return KAN_FALSE;
    }

    parsed_node->parent_expression = node->parent_expression;
    parse_expression_replace_in_parent (expression_parse_state, node, parsed_node);
    parser_expression_tree_node_destroy (node);

    if (*state->cursor != ')')
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Expected closing brace \")\" at the end of child grouped expression.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    ++state->cursor;
    expression_parse_state->expecting_operand = KAN_FALSE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_unary_operation (struct rpl_parser_t *parser,
                                                    struct dynamic_parser_state_t *state,
                                                    struct expression_parse_state_t *expression_parse_state,
                                                    enum kan_rpl_unary_operation_t operation)
{
    if (!expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Encountered unary operation while expecting binary operation.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    struct parser_expression_tree_node_t *node = expression_parse_state->current_node;
    KAN_ASSERT (node && node->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)

    struct parser_expression_tree_node_t *operation_node = parser_expression_tree_node_create (
        KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION, state->source_log_name, state->cursor_line);
    operation_node->unary_operation.unary_operation = operation;

    parse_expression_replace_in_parent (expression_parse_state, node, operation_node);
    operation_node->parent_expression = node->parent_expression;
    node->parent_expression = operation_node;
    operation_node->unary_operation.operand_expression = node;
    expression_parse_state->current_node = node;

    expression_parse_state->expecting_operand = KAN_TRUE;
    return KAN_TRUE;
}

static kan_bool_t parse_expression_binary_operation (struct rpl_parser_t *parser,
                                                     struct dynamic_parser_state_t *state,
                                                     struct expression_parse_state_t *expression_parse_state,
                                                     enum kan_rpl_binary_operation_t operation)
{
    if (expression_parse_state->expecting_operand)
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Encountered binary operation while expecting operand.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        return KAN_FALSE;
    }

    const uint64_t priority = binary_operation_priority[operation];
    struct parser_expression_tree_node_t *operation_node = parser_expression_tree_node_create (
        KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION, state->source_log_name, state->cursor_line);
    operation_node->binary_operation.binary_operation = operation;

    struct parser_expression_tree_node_t *next_operand_placeholder = parser_expression_tree_node_create (
        KAN_RPL_EXPRESSION_NODE_TYPE_NOPE, state->source_log_name, state->cursor_line);
    next_operand_placeholder->parent_expression = operation_node;

    switch (binary_operation_direction[operation])
    {
    case BINARY_OPERATION_DIRECTION_LEFT_TO_RIGHT:
    {
        struct parser_expression_tree_node_t *operation_parent =
            expression_parse_state->current_node->parent_expression;
        struct parser_expression_tree_node_t *child_to_replace = expression_parse_state->current_node;

        while (operation_parent)
        {
            if (operation_parent->type == KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION)
            {
                if (priority > binary_operation_priority[operation_parent->binary_operation.binary_operation])
                {
                    break;
                }
            }
            else if (operation_parent->type == KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION)
            {
                if (priority > unary_operation_priority)
                {
                    break;
                }
            }
            else
            {
                // Unexpected type of expression parent node.
                KAN_ASSERT (KAN_FALSE)
            }

            child_to_replace = operation_parent;
            operation_parent = operation_parent->parent_expression;
        }

        parse_expression_replace_in_parent (expression_parse_state, child_to_replace, operation_node);
        operation_node->parent_expression = operation_parent;
        child_to_replace->parent_expression = operation_node;
        operation_node->binary_operation.left_operand_expression = child_to_replace;
        operation_node->binary_operation.right_operand_expression = next_operand_placeholder;
        expression_parse_state->current_node = next_operand_placeholder;

        expression_parse_state->expecting_operand = KAN_TRUE;
        return KAN_TRUE;
    }

    case BINARY_OPERATION_DIRECTION_RIGHT_TO_LEFT:
    {
        struct parser_expression_tree_node_t *operation_parent =
            expression_parse_state->current_node->parent_expression;
        struct parser_expression_tree_node_t *child_to_replace = expression_parse_state->current_node;

        while (operation_parent)
        {
            if (operation_parent->type == KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION)
            {
                if (priority >= binary_operation_priority[operation_parent->binary_operation.binary_operation])
                {
                    break;
                }
            }
            else if (operation_parent->type == KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION)
            {
                if (priority >= unary_operation_priority)
                {
                    break;
                }
            }
            else
            {
                // Unexpected type of expression parent node.
                KAN_ASSERT (KAN_FALSE)
            }

            child_to_replace = operation_parent;
            operation_parent = operation_parent->parent_expression;
        }

        parse_expression_replace_in_parent (expression_parse_state, child_to_replace, operation_node);
        operation_node->parent_expression = operation_parent;
        child_to_replace->parent_expression = operation_node;
        operation_node->binary_operation.left_operand_expression = child_to_replace;
        operation_node->binary_operation.right_operand_expression = next_operand_placeholder;
        expression_parse_state->current_node = next_operand_placeholder;

        expression_parse_state->expecting_operand = KAN_TRUE;
        return KAN_TRUE;
    }
    }

    // Unknown direction.
    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static struct parser_expression_tree_node_t *parse_expression (struct rpl_parser_t *parser,
                                                               struct dynamic_parser_state_t *state)
{
    struct expression_parse_state_t expression_parse_state = {
        .expecting_operand = KAN_TRUE,
        .current_node = parser_expression_tree_node_create (KAN_RPL_EXPRESSION_NODE_TYPE_NOPE, state->source_log_name,
                                                            state->cursor_line),
    };

    while (KAN_TRUE)
    {
        state->token = state->cursor;
        re2c_save_cursor (state);
        const char *name_begin;
        const char *name_end;
        const char *literal_begin;
        const char *literal_end;

#define GET_STATE_PARENT                                                                                               \
    struct parser_expression_tree_node_t *state_parent = expression_parse_state.current_node;                          \
    while (state_parent->parent_expression)                                                                            \
    {                                                                                                                  \
        state_parent = state_parent->parent_expression;                                                                \
    }

#define CHECKED(...)                                                                                                   \
    if (!(__VA_ARGS__))                                                                                                \
    {                                                                                                                  \
        GET_STATE_PARENT;                                                                                              \
        parser_expression_tree_node_destroy (state_parent);                                                            \
        return NULL;                                                                                                   \
    }                                                                                                                  \
    continue;

        /*!re2c
         @literal_begin "-"? [0-9]+ @literal_end
         {
             CHECKED (parse_expression_integer_literal (parser, state, &expression_parse_state,
                                                        literal_begin, literal_end))
         }

         @literal_begin "-"? [0-9]+ "." [0-9]+  @literal_end
         {
             CHECKED (parse_expression_floating_literal (parser, state, &expression_parse_state,
                                                         literal_begin, literal_end))
         }

         "." separator* @name_begin identifier @name_end
         {
             CHECKED (parse_expression_field_access (parser, state, &expression_parse_state,
                                                     name_begin, name_end))
         }

         "["
         {
             CHECKED (parse_expression_array_access (parser, state, &expression_parse_state))
         }

         @name_begin identifier @name_end separator* "{"
         {
             CHECKED (parse_expression_operand_constructor (parser, state, &expression_parse_state,
                                                            name_begin, name_end))
         }

         @name_begin identifier @name_end separator* "("
         {
             CHECKED (parse_expression_operand_function_call (parser, state, &expression_parse_state,
                                                              name_begin, name_end))
         }

         @name_begin identifier @name_end
         {
             CHECKED (parse_expression_operand_identifier (parser, state, &expression_parse_state,
                                                           name_begin, name_end))
         }

         "("
         {
             CHECKED (parse_expression_operand_child_grouped_expression (parser, state, &expression_parse_state))
         }

         "+"
         {
             if (expression_parse_state.expecting_operand)
             {
                 continue;
             }

             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_ADD))
         }

         "-"
         {
             if (expression_parse_state.expecting_operand)
             {
                 CHECKED (parse_expression_unary_operation (parser, state, &expression_parse_state,
                                                            KAN_RPL_UNARY_OPERATION_NEGATE))
             }
             else
             {
                 CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                             KAN_RPL_BINARY_OPERATION_SUBTRACT))
             }
         }

         "*"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_MULTIPLY))
         }

         "/"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_DIVIDE))
         }

         "%"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_MODULUS))
         }

         "="
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_ASSIGN))
         }

         "&&"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_AND))
         }

         "||"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_OR))
         }

         "=="
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_EQUAL))
         }

         "!="
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_NOT_EQUAL))
         }

         "<"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_LESS))
         }

         ">"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_GREATER))
         }

         "<="
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_LESS_OR_EQUAL))
         }

         ">="
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_GREATER_OR_EQUAL))
         }

         "&"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_BITWISE_AND))
         }

         "|"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_BITWISE_OR))
         }

         "^"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_BITWISE_XOR))
         }

         "<<"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_BITWISE_LSHIFT))
         }

         ">>"
         {
             CHECKED (parse_expression_binary_operation (parser, state, &expression_parse_state,
                                                         KAN_RPL_BINARY_OPERATION_BITWISE_RSHIFT))
         }

         "!"
         {
             CHECKED (parse_expression_unary_operation (parser, state, &expression_parse_state,
                                                        KAN_RPL_UNARY_OPERATION_NOT))
         }

         "~"
         {
             CHECKED (parse_expression_unary_operation (parser, state, &expression_parse_state,
                                                        KAN_RPL_UNARY_OPERATION_BITWISE_NOT))
         }

         separator+ { continue; }
         comment+ { continue; }

         "," | ")" | ";" | "]" | "}"
         {
             // Reached expression break sign, it means that we've finished parsing expression.
             if (expression_parse_state.expecting_operand)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Reached end of expression while waiting for next operand.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)

                 re2c_restore_saved_cursor (state);
                 GET_STATE_PARENT;
                 parser_expression_tree_node_destroy (state_parent);
                 return NULL;
             }

             re2c_restore_saved_cursor (state);
             GET_STATE_PARENT;
             return state_parent;
         }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown construct while parsing expression.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             GET_STATE_PARENT;
             parser_expression_tree_node_destroy (state_parent);
             return NULL;
         }

         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while parsing expression.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             GET_STATE_PARENT;
             parser_expression_tree_node_destroy (state_parent);
             return NULL;
         }
         */

#undef CHECKED
#undef GET_STATE_PARENT
    }
}

static kan_bool_t parse_call_arguments (struct rpl_parser_t *parser,
                                        struct dynamic_parser_state_t *state,
                                        struct parser_expression_tree_node_t *output)
{
    KAN_ASSERT (output->type == KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR ||
                output->type == KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL)
    struct parser_expression_list_item_t *previous_item = NULL;

    while (KAN_TRUE)
    {
        struct parser_expression_tree_node_t *expression = parse_expression (parser, state);
        if (!expression)
        {
            return KAN_FALSE;
        }

        struct parser_expression_list_item_t *item =
            kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_expression_list_item_t));
        item->next = NULL;
        item->expression = expression;

        if (previous_item)
        {
            previous_item->next = item;
        }
        else if (output->type == KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR)
        {
            output->constructor.arguments = item;
        }
        else if (output->type == KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL)
        {
            output->function_call.arguments = item;
        }

        previous_item = item;
        if (*state->cursor != ',')
        {
            break;
        }

        ++state->cursor;
    }

    return KAN_TRUE;
}

static kan_bool_t parse_declarations_finish_item (struct rpl_parser_t *parser,
                                                  struct dynamic_parser_state_t *state,
                                                  struct parser_declaration_data_t *declaration)
{
    struct parser_expression_list_item_t *array_size_list_last = NULL;
    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *name_begin;
        const char *name_end;

        /*!re2c
         "["
         {
             struct parser_expression_tree_node_t *array_size_expression = parse_expression (parser, state);
             if (!array_size_expression)
             {
                 return KAN_FALSE;
             }

             struct parser_expression_list_item_t *new_item =
                     kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_expression_list_item_t));
             new_item->next = NULL;
             new_item->expression = array_size_expression;

             if (array_size_list_last)
             {
                 array_size_list_last->next = new_item;
             }
             else
             {
                 declaration->array_size_list = new_item;
             }

             array_size_list_last = new_item;
             if (*state->cursor != ']')
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Encountered array size expression that is not finished by \"]\".",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 return KAN_FALSE;
             }

             ++state->cursor;
             continue;
         }

         @name_begin identifier @name_end separator*
         {
             declaration->name = kan_char_sequence_intern (name_begin, name_end);
             return KAN_TRUE;
         }

         separator+ { continue; }
         comment+ { continue; }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression while reading declaration list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return KAN_FALSE;
         }
         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while reading declaration list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return KAN_FALSE;
         }
         */
    }
}

static struct parser_declaration_meta_item_t *parse_declarations_meta (struct rpl_parser_t *parser,
                                                                       struct dynamic_parser_state_t *state)
{
    struct parser_declaration_meta_item_t *meta_item = NULL;
    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *name_begin;
        const char *name_end;

        /*!re2c
         @name_begin identifier @name_end separator* ("," | ")")
         {
             struct parser_declaration_meta_item_t *new_item = kan_allocate_batched (
                     rpl_parser_allocation_group, sizeof (struct parser_declaration_meta_item_t));
             new_item->meta = kan_char_sequence_intern (name_begin, name_end);
             new_item->next = meta_item;
             meta_item = new_item;

             const char last = *(state->cursor - 1u);
             if (last == ')')
             {
                 return meta_item;
             }

             continue;
         }

         separator+
         {
             continue;
         }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression while reading meta list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)

             parser_destroy_declaration_meta (meta_item);
             return NULL;
         }
         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while reading meta list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)

             parser_destroy_declaration_meta (meta_item);
             return NULL;
         }
         */

#undef DESTROY_META
    }
}

static struct parser_declaration_t *parse_declarations (struct rpl_parser_t *parser,
                                                        struct dynamic_parser_state_t *state,
                                                        enum parser_declaration_list_type_t list_type)
{
    struct parser_declaration_t *first_declaration = NULL;
    struct parser_declaration_t *last_declaration = NULL;
    struct parser_declaration_meta_item_t *detached_meta = NULL;

    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *name_begin;
        const char *name_end;

        /*!re2c
         "conditional" separator* "("
         {
             state->detached_conditional = parse_detached_conditional (parser, state);
             if (!state->detached_conditional)
             {
                 parser_declaration_destroy (first_declaration);
                 return NULL;
             }

             continue;
         }

         "meta" separator* "("
         {
             struct parser_declaration_meta_item_t *new_meta = parse_declarations_meta (parser, state);
             if (!new_meta)
             {
                 parser_declaration_destroy (first_declaration);
                 parser_destroy_declaration_meta (detached_meta);
                 return NULL;
             }

             if (detached_meta)
             {
                 struct parser_declaration_meta_item_t *last_meta = detached_meta;
                 while (last_meta->next)
                 {
                     last_meta = last_meta->next;
                 }

                 last_meta->next = new_meta;
             }
             else
             {
                 detached_meta = new_meta;
             }

             continue;
         }

         @name_begin identifier @name_end
         {
             struct parser_declaration_t *new_declaration = parser_declaration_create (
                     state->source_log_name, state->cursor_line);
             new_declaration->declaration.type = kan_char_sequence_intern (name_begin, name_end);

             if (last_declaration)
             {
                 last_declaration->next = new_declaration;
             }
             else
             {
                 first_declaration = new_declaration;
             }

             last_declaration = new_declaration;
             new_declaration->first_meta = detached_meta;
             detached_meta = NULL;

             new_declaration->conditional = state->detached_conditional;
             state->detached_conditional = NULL;

             if (!parse_declarations_finish_item (parser, state, &new_declaration->declaration))
             {
                 parser_declaration_destroy (first_declaration);
                 return NULL;
             }

             switch (list_type)
             {
                 case PARSER_DECLARATION_LIST_TYPE_FIELDS:
                 {
                     if (*state->cursor != ';')
                     {
                         KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                                  "[%s:%s] [%ld:%ld]: Encountered declaration that is not finished by \";\".",
                                  parser->log_name, state->source_log_name, (long) state->cursor_line,
                                  (long) state->cursor_symbol)
                         parser_declaration_destroy (first_declaration);
                         parser_destroy_declaration_meta (detached_meta);
                         return NULL;
                     }

                     ++state->cursor;
                     break;
                 }

                 case PARSER_DECLARATION_LIST_TYPE_ARGUMENTS:
                 {
                     if (*state->cursor == ')')
                     {
                         ++state->cursor;
                         if (detached_meta)
                         {
                             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                                     "[%s:%s] [%ld:%ld]: Encountered detached meta at the end of declaration list.",
                                     parser->log_name, state->source_log_name, (long) state->cursor_line,
                                     (long) state->cursor_symbol)
                             parser_declaration_destroy (first_declaration);
                             parser_destroy_declaration_meta (detached_meta);
                             return NULL;
                         }

                         return first_declaration;
                     }
                     else if (*state->cursor != ',')
                     {
                         KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                                  "[%s:%s] [%ld:%ld]: Encountered declaration that is not finished by \",\".",
                                  parser->log_name, state->source_log_name, (long) state->cursor_line,
                                  (long) state->cursor_symbol)
                         parser_declaration_destroy (first_declaration);
                         parser_destroy_declaration_meta (detached_meta);
                         return NULL;
                     }

                     break;
                 }
             }

             continue;
         }

         "}" separator* ";"
         {
             if (list_type == PARSER_DECLARATION_LIST_TYPE_FIELDS)
             {
                 if (detached_meta)
                 {
                     KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                             "[%s:%s] [%ld:%ld]: Encountered detached meta at the end of declaration list.",
                             parser->log_name, state->source_log_name, (long) state->cursor_line,
                             (long) state->cursor_symbol)
                     parser_declaration_destroy (first_declaration);
                     parser_destroy_declaration_meta (detached_meta);
                     return NULL;
                 }

                 return first_declaration;
             }

             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression while reading declaration list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)

             parser_declaration_destroy (first_declaration);
             parser_destroy_declaration_meta (detached_meta);
             return NULL;
         }

         "void" separator* ")"
         {
             if (list_type == PARSER_DECLARATION_LIST_TYPE_ARGUMENTS)
             {
                 if (first_declaration)
                 {
                     KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                             "[%s:%s] [%ld:%ld]: \"void\" argument is only allowed (and required) for functions without"
                              " arguments.",
                              parser->log_name, state->source_log_name, (long) state->cursor_line,
                              (long) state->cursor_symbol)

                     parser_declaration_destroy (first_declaration);
                     parser_destroy_declaration_meta (detached_meta);
                     return NULL;
                 }

                 // Create special empty declaration to indicate absence of arguments with successful parse.
                 first_declaration = parser_declaration_create (state->source_log_name, state->cursor_line);
                 first_declaration->declaration.type = interned_void;
                 return first_declaration;
             }

             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression while reading declaration list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)

             parser_declaration_destroy (first_declaration);
             parser_destroy_declaration_meta (detached_meta);
             return NULL;
         }

         separator+ { continue; }
         comment+ { continue; }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression while reading declaration list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)

             parser_declaration_destroy (first_declaration);
             parser_destroy_declaration_meta (detached_meta);
             return NULL;
         }
         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while reading declaration list.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)

             parser_declaration_destroy (first_declaration);
             parser_destroy_declaration_meta (detached_meta);
             return NULL;
         }
         */
    }
}

static inline kan_bool_t parse_sampler_setting_flag (struct rpl_parser_t *parser,
                                                     struct dynamic_parser_state_t *state,
                                                     struct parser_sampler_t *sampler,
                                                     const char *name_begin,
                                                     const char *name_end,
                                                     kan_bool_t value)
{
    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    struct parser_setting_list_item_t *item =
        kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_setting_t));

    item->setting.name = name;
    item->setting.type = KAN_RPL_SETTING_TYPE_FLAG;
    item->setting.flag = value;

    item->setting.conditional = state->detached_conditional;
    state->detached_conditional = NULL;

    item->setting.source_log_name = state->source_log_name;
    item->setting.source_line = state->cursor_line;

    // We can push to the beginning as order of settings should not matter.
    item->next = sampler->first_setting;
    sampler->first_setting = item;

    return KAN_TRUE;
}

static inline kan_bool_t parse_sampler_setting_integer (struct rpl_parser_t *parser,
                                                        struct dynamic_parser_state_t *state,
                                                        struct parser_sampler_t *sampler,
                                                        const char *name_begin,
                                                        const char *name_end,
                                                        const char *literal_begin,
                                                        const char *literal_end)
{
    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    struct parser_setting_list_item_t *item =
        kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_setting_t));

    item->setting.name = name;
    item->setting.type = KAN_RPL_SETTING_TYPE_INTEGER;

    const kan_bool_t negative = *literal_begin == '-';
    if (negative)
    {
        ++literal_begin;
    }

    const uint64_t unsigned_value = parse_unsigned_integer_value (parser, state, literal_begin, literal_end);
    if (unsigned_value > INT64_MAX)
    {
        KAN_LOG (rpl_parser, KAN_LOG_WARNING, "[%s:%s] [%ld:%ld]: Setting \"%s\" integer value is too big.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol, name)
        return KAN_FALSE;
    }

    item->setting.integer = (int64_t) unsigned_value;
    if (negative)
    {
        item->setting.integer = -item->setting.integer;
    }

    item->setting.conditional = state->detached_conditional;
    state->detached_conditional = NULL;

    item->setting.source_log_name = state->source_log_name;
    item->setting.source_line = state->cursor_line;

    // We can push to the beginning as order of settings should not matter.
    item->next = sampler->first_setting;
    sampler->first_setting = item;

    return KAN_TRUE;
}

static inline kan_bool_t parse_sampler_setting_floating (struct rpl_parser_t *parser,
                                                         struct dynamic_parser_state_t *state,
                                                         struct parser_sampler_t *sampler,
                                                         const char *name_begin,
                                                         const char *name_end,
                                                         const char *literal_begin,
                                                         const char *literal_end)
{
    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    struct parser_setting_list_item_t *item =
        kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_setting_t));

    item->setting.name = name;
    item->setting.type = KAN_RPL_SETTING_TYPE_FLOATING;

    const kan_bool_t negative = *literal_begin == '-';
    if (negative)
    {
        ++literal_begin;
    }

    const double unsigned_value = parse_unsigned_floating_value (parser, state, literal_begin, literal_end);
    item->setting.floating = unsigned_value;

    if (negative)
    {
        item->setting.floating = -item->setting.floating;
    }

    item->setting.conditional = state->detached_conditional;
    state->detached_conditional = NULL;

    item->setting.source_log_name = state->source_log_name;
    item->setting.source_line = state->cursor_line;

    // We can push to the beginning as order of settings should not matter.
    item->next = sampler->first_setting;
    sampler->first_setting = item;

    return KAN_TRUE;
}

static inline kan_bool_t parse_sampler_setting_string (struct rpl_parser_t *parser,
                                                       struct dynamic_parser_state_t *state,
                                                       struct parser_sampler_t *sampler,
                                                       const char *name_begin,
                                                       const char *name_end,
                                                       const char *literal_begin,
                                                       const char *literal_end)
{
    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    struct parser_setting_list_item_t *item =
        kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_setting_t));

    item->setting.name = name;
    item->setting.type = KAN_RPL_SETTING_TYPE_STRING;
    item->setting.string = kan_char_sequence_intern (literal_begin, literal_end);

    item->setting.conditional = state->detached_conditional;
    state->detached_conditional = NULL;

    item->setting.source_log_name = state->source_log_name;
    item->setting.source_line = state->cursor_line;

    // We can push to the beginning as order of settings should not matter.
    item->next = sampler->first_setting;
    sampler->first_setting = item;

    return KAN_TRUE;
}

static inline kan_bool_t parse_sampler_settings (struct rpl_parser_t *parser,
                                                 struct dynamic_parser_state_t *state,
                                                 struct parser_sampler_t *sampler)
{
    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *name_begin;
        const char *name_end;
        const char *literal_begin;
        const char *literal_end;

#define CHECKED(...)                                                                                                   \
    if (!(__VA_ARGS__))                                                                                                \
    {                                                                                                                  \
        return KAN_FALSE;                                                                                              \
    }                                                                                                                  \
    continue;

        /*!re2c
         "conditional" separator* "("
         {
             state->detached_conditional = parse_detached_conditional (parser, state);
             if (!state->detached_conditional)
             {
                 return KAN_FALSE;
             }

             continue;
         }

         "setting" separator+ @name_begin (identifier | ".")+ @name_end separator+ "on" separator* ";"
         { CHECKED (parse_sampler_setting_flag (parser, state, sampler, name_begin, name_end, KAN_TRUE)) }

         "setting" separator+ @name_begin (identifier | ".")+ @name_end separator+ "off" separator* ";"
         { CHECKED (parse_sampler_setting_flag (parser, state, sampler, name_begin, name_end, KAN_FALSE)) }

         "setting" separator+ @name_begin (identifier | ".")+ @name_end separator+
         @literal_begin "-"? [0-9]+ @literal_end separator* ";"
         {
             CHECKED (parse_sampler_setting_integer (
                     parser, state, sampler, name_begin, name_end, literal_begin, literal_end))
         }

         "setting" separator+ @name_begin (identifier | ".")+ @name_end separator+
         @literal_begin "-"? [0-9]+ "." [0-9]+ @literal_end separator* ";"
         {
             CHECKED (parse_sampler_setting_floating (
                     parser, state, sampler, name_begin, name_end, literal_begin, literal_end))
         }

         "setting" separator+ @name_begin (identifier | ".")+ @name_end separator+
         "\"" @literal_begin ((. \ [\x22]) | "\\\"")* @literal_end "\"" separator* ";"
         {
             CHECKED (parse_sampler_setting_string (
                     parser, state, sampler, name_begin, name_end, literal_begin, literal_end))
         }

         "}" separator* ";" { return KAN_TRUE; }

         separator+ { continue; }
         comment+ { continue; }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression while reading sampler settings.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return KAN_FALSE;
         }
         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while reading sampler settings.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return KAN_FALSE;
         }
         */

#undef CHECKED
    }
}

static kan_bool_t parse_main_sampler (struct rpl_parser_t *parser,
                                      struct dynamic_parser_state_t *state,
                                      enum kan_rpl_sampler_type_t type,
                                      const char *name_begin,
                                      const char *name_end)
{
    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    if (!ensure_sampler_name_unique (parser, state, name))
    {
        return KAN_FALSE;
    }

    struct parser_sampler_t *sampler = parser_sampler_create (name, type, state->source_log_name, state->cursor_line);
    sampler->conditional = state->detached_conditional;
    state->detached_conditional = NULL;

    if (parse_sampler_settings (parser, state, sampler))
    {
        kan_hash_storage_update_bucket_count_default (&parser->processing_data.samplers,
                                                      KAN_RPL_PARSER_SAMPLERS_INITIAL_BUCKETS);
        kan_hash_storage_add (&parser->processing_data.samplers, &sampler->node);
        return KAN_TRUE;
    }
    else
    {
        parser_sampler_destroy (sampler);
        return KAN_FALSE;
    }
}

static struct parser_expression_tree_node_t *parse_scope (struct rpl_parser_t *parser,
                                                          struct dynamic_parser_state_t *state);

static struct parser_expression_tree_node_t *expect_scope (struct rpl_parser_t *parser,
                                                           struct dynamic_parser_state_t *state)
{
    while (KAN_TRUE)
    {
        state->token = state->cursor;
        /*!re2c
         "{" { return parse_scope (parser, state); }
         separator+ { continue; }
         comment+ { continue; }

         *
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered unknown expression while expecting new code scope.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return NULL;
         }
         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while expecting new code scope.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             return NULL;
         }
         */
    }
}

static kan_interned_string_t expect_variable_declaration_type (struct rpl_parser_t *parser,
                                                               struct dynamic_parser_state_t *state)
{
    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *name_begin;
        const char *name_end;

        /*!re2c
         @name_begin identifier @name_end { return kan_char_sequence_intern (name_begin, name_end); }

         separator+ { continue; }
         comment+ { continue; }

        *
        {
            KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                     "[%s:%s] [%ld:%ld]: Encountered unknown expression while expecting variable declaration type.",
                     parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
            return NULL;
        }
        $
        {
            KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                     "[%s:%s] [%ld:%ld]: Encountered end of file while expecting variable declaration type.",
                     parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
            return NULL;
        }
        */
    }
}

static struct parser_expression_tree_node_t *expect_variable_declaration (struct rpl_parser_t *parser,
                                                                          struct dynamic_parser_state_t *state)
{
    kan_interned_string_t type_name = expect_variable_declaration_type (parser, state);
    if (!type_name)
    {
        return NULL;
    }

    struct parser_expression_tree_node_t *variable_declaration = parser_expression_tree_node_create (
        KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION, state->source_log_name, state->saved_line);
    variable_declaration->variable_declaration.type = type_name;

    if (!parse_declarations_finish_item (parser, state, &variable_declaration->variable_declaration))
    {
        parser_expression_tree_node_destroy (variable_declaration);
        return NULL;
    }

    if (*state->cursor == '=')
    {
        ++state->cursor;
        struct parser_expression_tree_node_t *assignment = parser_expression_tree_node_create (
            KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION, state->source_log_name, state->saved_line);
        assignment->binary_operation.binary_operation = KAN_RPL_BINARY_OPERATION_ASSIGN;
        assignment->binary_operation.left_operand_expression = variable_declaration;
        assignment->binary_operation.right_operand_expression = parse_expression (parser, state);

        if (!assignment->binary_operation.right_operand_expression)
        {
            parser_expression_tree_node_destroy (assignment);
            return NULL;
        }

        if (*state->cursor != ';')
        {
            KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                     "[%s:%s] [%ld:%ld]: Variable declaration with assignment should be finished with \";\".",
                     parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
            parser_expression_tree_node_destroy (assignment);
            return NULL;
        }

        ++state->cursor;
        return assignment;
    }
    else if (*state->cursor == ';')
    {
        ++state->cursor;
        return variable_declaration;
    }
    else
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                 "[%s:%s] [%ld:%ld]: Expected either \";\" or \"=\" after variable name in declaration.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        parser_expression_tree_node_destroy (variable_declaration);
        return NULL;
    }
}

static struct parser_expression_tree_node_t *parse_if_after_keyword (struct rpl_parser_t *parser,
                                                                     struct dynamic_parser_state_t *state)
{
    struct parser_expression_tree_node_t *condition_expression = parse_expression (parser, state);

    if (!condition_expression)
    {
        return NULL;
    }

    if (*state->cursor != ')')
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Expected \")\" after if condition.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        parser_expression_tree_node_destroy (condition_expression);
        return NULL;
    }

    ++state->cursor;
    struct parser_expression_tree_node_t *if_expression = parser_expression_tree_node_create (
        KAN_RPL_EXPRESSION_NODE_TYPE_IF, state->source_log_name, state->cursor_line);
    if_expression->if_.condition_expression = condition_expression;
    if_expression->if_.true_expression = expect_scope (parser, state);

    if (!if_expression->if_.true_expression)
    {
        parser_expression_tree_node_destroy (if_expression);
        return NULL;
    }

    // Match continuation: either else or else if or nothing.
    while (KAN_TRUE)
    {
        state->token = state->cursor;
        re2c_save_cursor (state);

        /*!re2c
         "else" separator+ "if" separator* "("
         {
             if_expression->if_.false_expression = parse_if_after_keyword (parser, state);
             if (!if_expression->if_.false_expression)
             {
                 parser_expression_tree_node_destroy (if_expression);
                 return NULL;
             }

             return if_expression;
         }

         "else"
         {
             if_expression->if_.false_expression = expect_scope (parser, state);
             if (!if_expression->if_.false_expression)
             {
                 parser_expression_tree_node_destroy (if_expression);
                 return NULL;
             }

             return if_expression;
         }

         separator+ { continue; }
         comment+ { continue; }
         * { re2c_restore_saved_cursor (state); return if_expression; }
         $ { re2c_restore_saved_cursor (state); return if_expression; }
         */
    }
}

static struct parser_expression_tree_node_t *parse_for_after_keyword (struct rpl_parser_t *parser,
                                                                      struct dynamic_parser_state_t *state)
{
    struct parser_expression_tree_node_t *for_expression = parser_expression_tree_node_create (
        KAN_RPL_EXPRESSION_NODE_TYPE_FOR, state->source_log_name, state->cursor_line);
    for_expression->for_.init_expression = expect_variable_declaration (parser, state);

    if (!for_expression->for_.init_expression)
    {
        parser_expression_tree_node_destroy (for_expression);
        return NULL;
    }

    for_expression->for_.condition_expression = parse_expression (parser, state);
    if (!for_expression->for_.condition_expression)
    {
        parser_expression_tree_node_destroy (for_expression);
        return NULL;
    }

    if (*state->cursor != ';')
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Expected \";\" after for condition.", parser->log_name,
                 state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        parser_expression_tree_node_destroy (for_expression);
        return NULL;
    }

    ++state->cursor;
    for_expression->for_.step_expression = parse_expression (parser, state);

    if (!for_expression->for_.step_expression)
    {
        parser_expression_tree_node_destroy (for_expression);
        return NULL;
    }

    if (*state->cursor != ')')
    {
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: Expected \")\" after for step expression.",
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
        parser_expression_tree_node_destroy (for_expression);
        return NULL;
    }

    ++state->cursor;
    for_expression->for_.body_expression = expect_scope (parser, state);

    if (!for_expression->for_.body_expression)
    {
        parser_expression_tree_node_destroy (for_expression);
        return NULL;
    }

    return for_expression;
}

static struct parser_expression_tree_node_t *parse_scope (struct rpl_parser_t *parser,
                                                          struct dynamic_parser_state_t *state)
{
    struct parser_expression_tree_node_t *scope_expression = parser_expression_tree_node_create (
        KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE, state->source_log_name, state->saved_line);
    struct parser_expression_list_item_t *last_item = NULL;

#define DOES_NOT_SUPPORT_CONDITIONAL                                                                                   \
    if (state->detached_conditional)                                                                                   \
    {                                                                                                                  \
        KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld:%ld]: This expression does not support conditional prefix.", \
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)     \
                                                                                                                       \
        parser_expression_tree_node_destroy (scope_expression);                                                        \
        return NULL;                                                                                                   \
    }

#define CHECK_EXPRESSION_SEMICOLON                                                                                     \
    if (*state->cursor != ';')                                                                                         \
    {                                                                                                                  \
        KAN_LOG (rpl_parser, KAN_LOG_ERROR,                                                                            \
                 "[%s:%s] [%ld:%ld]: Encountered expression that is not finished by \";\" as expected.",               \
                 parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)     \
                                                                                                                       \
        parser_expression_tree_node_destroy (scope_expression);                                                        \
        return NULL;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    ++state->cursor

#define ADD_EXPRESSION(EXPRESSION)                                                                                     \
    struct parser_expression_list_item_t *new_item =                                                                   \
        kan_allocate_batched (rpl_parser_allocation_group, sizeof (struct parser_expression_list_item_t));             \
    new_item->next = NULL;                                                                                             \
    new_item->expression = EXPRESSION;                                                                                 \
                                                                                                                       \
    if (last_item)                                                                                                     \
    {                                                                                                                  \
        last_item->next = new_item;                                                                                    \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        scope_expression->scope_expressions_list = new_item;                                                           \
    }                                                                                                                  \
                                                                                                                       \
    last_item = new_item

    while (KAN_TRUE)
    {
        state->token = state->cursor;
        const char *name_begin;
        const char *name_end;
        re2c_save_cursor (state);

        /*!re2c
         "conditional" separator* "("
         {
             state->detached_conditional = parse_detached_conditional (parser, state);
             if (!state->detached_conditional)
             {
                 parser_expression_tree_node_destroy (scope_expression);
                 return NULL;
             }

             continue;
         }

         ("return" separator+ @name_begin identifier @name_end separator* ";") |
         (identifier separator+ identifier separator* (";" | "=")) |
         (identifier separator* "[")
         {
             DOES_NOT_SUPPORT_CONDITIONAL

             // Parser ad-hoc: "return variable;" looks like a declaration for the parser.
             if (name_begin)
             {
                 struct parser_expression_tree_node_t *return_expression =
                 parser_expression_tree_node_create (KAN_RPL_EXPRESSION_NODE_TYPE_RETURN, state->source_log_name,
                                                     state->saved_line);
                 return_expression->return_expression =
                         parser_expression_tree_node_create (KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER,
                                 state->source_log_name, state->saved_line);
                 return_expression->return_expression->identifier = kan_char_sequence_intern (name_begin, name_end);

                 ADD_EXPRESSION (return_expression);
                 continue;
             }

             // We've encountered something that definitely looks like part of
             // variable declaration (not a function call of anything else).
             // In this case we restore cursor and read it properly.
             re2c_restore_saved_cursor (state);
             struct parser_expression_tree_node_t *declaration = expect_variable_declaration (parser, state);

             if (!declaration)
             {
                 parser_expression_tree_node_destroy (scope_expression);
                 return NULL;
             }

             ADD_EXPRESSION (declaration);
             continue;
         }

         "{"
         {
             if (state->detached_conditional)
             {
                 struct parser_expression_tree_node_t *expression =
                     parser_expression_tree_node_create (KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE,
                                                         state->source_log_name, state->saved_line);
                 expression->conditional_scope.condition_expression = state->detached_conditional;
                 state->detached_conditional = NULL;
                 expression->conditional_scope.body_expression = parse_scope (parser, state);

                 if (!expression->conditional_scope.body_expression)
                 {
                     parser_expression_tree_node_destroy (expression);
                     parser_expression_tree_node_destroy (scope_expression);
                     return NULL;
                 }

                 ADD_EXPRESSION (expression);
             }
             else
             {
                 struct parser_expression_tree_node_t *new_scope = parse_scope (parser, state);
                 if (!new_scope)
                 {
                     parser_expression_tree_node_destroy (scope_expression);
                     return NULL;
                 }

                 ADD_EXPRESSION (new_scope);
             }

             continue;
         }

         "if" separator* "("
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             struct parser_expression_tree_node_t *expression = parse_if_after_keyword (parser, state);

             if (!expression)
             {
                 parser_expression_tree_node_destroy (scope_expression);
                 return NULL;
             }

             ADD_EXPRESSION (expression);
             continue;
         }

         "for" separator* "("
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             struct parser_expression_tree_node_t *expression = parse_for_after_keyword (parser, state);

             if (!expression)
             {
                 parser_expression_tree_node_destroy (scope_expression);
                 return NULL;
             }

             ADD_EXPRESSION (expression);
             continue;
         }

         "while" separator* "("
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             struct parser_expression_tree_node_t *condition_expression = parse_expression (parser, state);

             if (!condition_expression)
             {
                 parser_expression_tree_node_destroy (scope_expression);
                 return NULL;
             }

             if (*state->cursor != ')')
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: While condition is not finished by \")\" as expected.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 parser_expression_tree_node_destroy (condition_expression);
                 parser_expression_tree_node_destroy (scope_expression);
                 return NULL;
             }

             ++state->cursor;
             struct parser_expression_tree_node_t *body_expression = expect_scope (parser, state);

             if (!body_expression)
             {
                  parser_expression_tree_node_destroy (condition_expression);
                  parser_expression_tree_node_destroy (scope_expression);
                  return NULL;
             }

             struct parser_expression_tree_node_t *while_expression =
                 parser_expression_tree_node_create (KAN_RPL_EXPRESSION_NODE_TYPE_WHILE, state->source_log_name,
                                                     state->saved_line);
             while_expression->while_.condition_expression = condition_expression;
             while_expression->while_.body_expression = body_expression;
             ADD_EXPRESSION (while_expression);
             continue;
         }

         "alias" separator* "(" separator* @name_begin identifier @name_end separator* ","
         {
             if (!state->detached_conditional)
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Alias expression must have conditional prefix.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 parser_expression_tree_node_destroy (scope_expression);
                 return NULL;
             }

             struct parser_expression_tree_node_t *body_expression = parse_expression (parser, state);
             if (!body_expression)
             {
                  parser_expression_tree_node_destroy (scope_expression);
                  return NULL;
             }

             struct parser_expression_tree_node_t *alias_expression =
                 parser_expression_tree_node_create (KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS,
                                                     state->source_log_name, state->saved_line);
             alias_expression->conditional_alias.condition_expression = state->detached_conditional;
             state->detached_conditional = NULL;
             alias_expression->conditional_alias.identifier = kan_char_sequence_intern (name_begin, name_end);
             alias_expression->conditional_alias.body_expression = body_expression;
             ADD_EXPRESSION (alias_expression);

             if (*state->cursor != ')')
             {
                 KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                          "[%s:%s] [%ld:%ld]: Alias expression is not finished by \")\" as expected.",
                          parser->log_name, state->source_log_name, (long) state->cursor_line,
                          (long) state->cursor_symbol)
                 parser_expression_tree_node_destroy (scope_expression);
                 return NULL;
             }

             ++state->cursor;
             continue;
         }

         "break" separator* ";"
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             struct parser_expression_tree_node_t *break_expression =
                 parser_expression_tree_node_create (KAN_RPL_EXPRESSION_NODE_TYPE_BREAK, state->source_log_name,
                                                     state->saved_line);
             ADD_EXPRESSION (break_expression);
             continue;
         }

         "continue" separator* ";"
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             struct parser_expression_tree_node_t *continue_expression =
                 parser_expression_tree_node_create (KAN_RPL_EXPRESSION_NODE_TYPE_CONTINUE, state->source_log_name,
                                                     state->saved_line);
             ADD_EXPRESSION (continue_expression);
             continue;
         }

         "return" separator* ";"
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             struct parser_expression_tree_node_t *return_expression =
                 parser_expression_tree_node_create (KAN_RPL_EXPRESSION_NODE_TYPE_RETURN, state->source_log_name,
                                                     state->saved_line);
             return_expression->return_expression = NULL;

             ADD_EXPRESSION (return_expression);
             CHECK_EXPRESSION_SEMICOLON;
             continue;
         }

         "return" separator+
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             struct parser_expression_tree_node_t *value_expression = parse_expression (parser, state);
             if (!value_expression)
             {
                  parser_expression_tree_node_destroy (scope_expression);
                  return NULL;
             }

             struct parser_expression_tree_node_t *return_expression =
                 parser_expression_tree_node_create (KAN_RPL_EXPRESSION_NODE_TYPE_RETURN, state->source_log_name,
                                                     state->saved_line);
             return_expression->return_expression = value_expression;

             ADD_EXPRESSION (return_expression);
             CHECK_EXPRESSION_SEMICOLON;
             continue;
         }

         *
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             // Not a prefixed expression (like if, for, etc), parse as just an expression.
             re2c_restore_saved_cursor (state);
             struct parser_expression_tree_node_t *next_expression = parse_expression (parser, state);

             if (!next_expression)
             {
                  parser_expression_tree_node_destroy (scope_expression);
                  return NULL;
             }

             ADD_EXPRESSION (next_expression);
             CHECK_EXPRESSION_SEMICOLON;
             continue;
         }

         "}"
         {
             DOES_NOT_SUPPORT_CONDITIONAL
             // Scope has ended.
             return scope_expression;
         }

         separator+ { continue; }
         comment+ { continue; }

         $
         {
             KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                      "[%s:%s] [%ld:%ld]: Encountered end of file while reading scope block.",
                      parser->log_name, state->source_log_name, (long) state->cursor_line, (long) state->cursor_symbol)
             parser_expression_tree_node_destroy (scope_expression);
             return NULL;
         }
         */
    }

#undef ADD_EXPRESSION
#undef CHECK_EXPRESSION_SEMICOLON
#undef DOES_NOT_SUPPORT_CONDITIONAL
}

static kan_bool_t parse_main_function (struct rpl_parser_t *parser,
                                       struct dynamic_parser_state_t *state,
                                       const char *name_begin,
                                       const char *name_end,
                                       const char *type_name_begin,
                                       const char *type_name_end)
{
    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    kan_interned_string_t type_name = kan_char_sequence_intern (type_name_begin, type_name_end);

    if (!ensure_function_name_unique (parser, state, name))
    {
        return KAN_FALSE;
    }

    struct parser_function_t *function =
        parser_function_create (name, type_name, state->source_log_name, state->cursor_line);
    function->conditional = state->detached_conditional;
    state->detached_conditional = NULL;
    function->first_argument = parse_declarations (parser, state, PARSER_DECLARATION_LIST_TYPE_ARGUMENTS);

    if (!function->first_argument)
    {
        parser_function_destroy (function);
        return KAN_FALSE;
    }

    function->body_expression = expect_scope (parser, state);
    if (function->body_expression)
    {
        kan_hash_storage_update_bucket_count_default (&parser->processing_data.functions,
                                                      KAN_RPL_PARSER_FUNCTIONS_INITIAL_BUCKETS);
        kan_hash_storage_add (&parser->processing_data.functions, &function->node);
        return KAN_TRUE;
    }
    else
    {
        parser_function_destroy (function);
        return KAN_FALSE;
    }
}

void kan_rpl_expression_node_init (struct kan_rpl_expression_node_t *instance)
{
    instance->type = KAN_RPL_EXPRESSION_NODE_TYPE_NOPE;
    kan_dynamic_array_init (&instance->children, 0u, sizeof (struct kan_rpl_expression_node_t),
                            _Alignof (struct kan_rpl_expression_node_t), rpl_intermediate_allocation_group);
    instance->source_name = NULL;
    instance->source_line = 0u;
}

void kan_rpl_expression_node_shutdown (struct kan_rpl_expression_node_t *instance)
{
    for (uint64_t index = 0u; index < instance->children.size; ++index)
    {
        kan_rpl_expression_node_shutdown (&((struct kan_rpl_expression_node_t *) instance->children.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->children);
}

void kan_rpl_setting_init (struct kan_rpl_setting_t *instance)
{
    instance->name = NULL;
    instance->type = KAN_RPL_SETTING_TYPE_FLAG;
    instance->flag = KAN_FALSE;
    kan_rpl_expression_node_init (&instance->conditional);
    instance->source_name = NULL;
    instance->source_line = 0u;
}

void kan_rpl_setting_shutdown (struct kan_rpl_setting_t *instance)
{
    kan_rpl_expression_node_shutdown (&instance->conditional);
}

void kan_rpl_declaration_init (struct kan_rpl_declaration_t *instance)
{
    instance->type_name = NULL;
    instance->name = NULL;
    kan_dynamic_array_init (&instance->array_sizes, 0u, sizeof (struct kan_rpl_expression_node_t),
                            _Alignof (struct kan_rpl_expression_node_t), rpl_intermediate_allocation_group);
    kan_dynamic_array_init (&instance->meta, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            rpl_intermediate_allocation_group);
    kan_rpl_expression_node_init (&instance->conditional);
    instance->source_name = NULL;
    instance->source_line = 0u;
}

void kan_rpl_declaration_shutdown (struct kan_rpl_declaration_t *instance)
{
    for (uint64_t index = 0u; index < instance->array_sizes.size; ++index)
    {
        kan_rpl_expression_node_shutdown (&((struct kan_rpl_expression_node_t *) instance->array_sizes.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->array_sizes);
    kan_dynamic_array_shutdown (&instance->meta);
    kan_rpl_expression_node_shutdown (&instance->conditional);
}

void kan_rpl_struct_init (struct kan_rpl_struct_t *instance)
{
    instance->name = NULL;
    kan_dynamic_array_init (&instance->fields, 0u, sizeof (struct kan_rpl_declaration_t),
                            _Alignof (struct kan_rpl_declaration_t), rpl_intermediate_allocation_group);
    kan_rpl_expression_node_init (&instance->conditional);
    instance->source_name = NULL;
    instance->source_line = 0u;
}

void kan_rpl_struct_shutdown (struct kan_rpl_struct_t *instance)
{
    for (uint64_t index = 0u; index < instance->fields.size; ++index)
    {
        kan_rpl_declaration_shutdown (&((struct kan_rpl_declaration_t *) instance->fields.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->fields);
    kan_rpl_expression_node_shutdown (&instance->conditional);
}

void kan_rpl_buffer_init (struct kan_rpl_buffer_t *instance)
{
    instance->name = NULL;
    instance->type = KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE;
    kan_dynamic_array_init (&instance->fields, 0u, sizeof (struct kan_rpl_declaration_t),
                            _Alignof (struct kan_rpl_declaration_t), rpl_intermediate_allocation_group);
    kan_rpl_expression_node_init (&instance->conditional);
    instance->source_name = NULL;
    instance->source_line = 0u;
}

void kan_rpl_buffer_shutdown (struct kan_rpl_buffer_t *instance)
{
    for (uint64_t index = 0u; index < instance->fields.size; ++index)
    {
        kan_rpl_declaration_shutdown (&((struct kan_rpl_declaration_t *) instance->fields.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->fields);
    kan_rpl_expression_node_shutdown (&instance->conditional);
}

void kan_rpl_sampler_init (struct kan_rpl_sampler_t *instance)
{
    instance->name = NULL;
    instance->type = KAN_RPL_SAMPLER_TYPE_2D;
    kan_dynamic_array_init (&instance->settings, 0u, sizeof (struct kan_rpl_setting_t),
                            _Alignof (struct kan_rpl_setting_t), rpl_intermediate_allocation_group);
    kan_rpl_expression_node_init (&instance->conditional);
    instance->source_name = NULL;
    instance->source_line = 0u;
}

void kan_rpl_sampler_shutdown (struct kan_rpl_sampler_t *instance)
{
    for (uint64_t index = 0u; index < instance->settings.size; ++index)
    {
        kan_rpl_setting_shutdown (&((struct kan_rpl_setting_t *) instance->settings.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->settings);
    kan_rpl_expression_node_shutdown (&instance->conditional);
}

void kan_rpl_function_init (struct kan_rpl_function_t *instance)
{
    instance->return_type_name = NULL;
    instance->name = NULL;
    kan_dynamic_array_init (&instance->arguments, 0u, sizeof (struct kan_rpl_declaration_t),
                            _Alignof (struct kan_rpl_declaration_t), rpl_intermediate_allocation_group);
    kan_rpl_expression_node_init (&instance->body);
    kan_rpl_expression_node_init (&instance->conditional);
    instance->source_name = NULL;
    instance->source_line = 0u;
}

void kan_rpl_function_shutdown (struct kan_rpl_function_t *instance)
{
    for (uint64_t index = 0u; index < instance->arguments.size; ++index)
    {
        kan_rpl_declaration_shutdown (&((struct kan_rpl_declaration_t *) instance->arguments.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->arguments);
    kan_rpl_expression_node_shutdown (&instance->body);
    kan_rpl_expression_node_shutdown (&instance->conditional);
}

void kan_rpl_intermediate_init (struct kan_rpl_intermediate_t *instance)
{
    ensure_statics_initialized ();
    kan_dynamic_array_init (&instance->options, 0u, sizeof (struct kan_rpl_option_t),
                            _Alignof (struct kan_rpl_option_t), rpl_intermediate_allocation_group);
    kan_dynamic_array_init (&instance->settings, 0u, sizeof (struct kan_rpl_setting_t),
                            _Alignof (struct kan_rpl_setting_t), rpl_intermediate_allocation_group);
    kan_dynamic_array_init (&instance->structs, 0u, sizeof (struct kan_rpl_struct_t),
                            _Alignof (struct kan_rpl_struct_t), rpl_intermediate_allocation_group);
    kan_dynamic_array_init (&instance->buffers, 0u, sizeof (struct kan_rpl_buffer_t),
                            _Alignof (struct kan_rpl_buffer_t), rpl_intermediate_allocation_group);
    kan_dynamic_array_init (&instance->samplers, 0u, sizeof (struct kan_rpl_sampler_t),
                            _Alignof (struct kan_rpl_sampler_t), rpl_intermediate_allocation_group);
    kan_dynamic_array_init (&instance->functions, 0u, sizeof (struct kan_rpl_function_t),
                            _Alignof (struct kan_rpl_function_t), rpl_intermediate_allocation_group);
}

void kan_rpl_intermediate_shutdown (struct kan_rpl_intermediate_t *instance)
{
    for (uint64_t index = 0u; index < instance->settings.size; ++index)
    {
        kan_rpl_setting_shutdown (&((struct kan_rpl_setting_t *) instance->settings.data)[index]);
    }

    for (uint64_t index = 0u; index < instance->structs.size; ++index)
    {
        kan_rpl_struct_shutdown (&((struct kan_rpl_struct_t *) instance->structs.data)[index]);
    }

    for (uint64_t index = 0u; index < instance->buffers.size; ++index)
    {
        kan_rpl_buffer_shutdown (&((struct kan_rpl_buffer_t *) instance->buffers.data)[index]);
    }

    for (uint64_t index = 0u; index < instance->samplers.size; ++index)
    {
        kan_rpl_sampler_shutdown (&((struct kan_rpl_sampler_t *) instance->samplers.data)[index]);
    }

    for (uint64_t index = 0u; index < instance->functions.size; ++index)
    {
        kan_rpl_function_shutdown (&((struct kan_rpl_function_t *) instance->functions.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->options);
    kan_dynamic_array_shutdown (&instance->settings);
    kan_dynamic_array_shutdown (&instance->structs);
    kan_dynamic_array_shutdown (&instance->buffers);
    kan_dynamic_array_shutdown (&instance->samplers);
    kan_dynamic_array_shutdown (&instance->functions);
}

void kan_rpl_meta_parameter_init (struct kan_rpl_meta_parameter_t *instance)
{
    instance->name = NULL;
    instance->type = KAN_RPL_META_VARIABLE_TYPE_F1;
    instance->offset = 0u;
    instance->total_item_count = 0u;
    kan_dynamic_array_init (&instance->meta, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            rpl_meta_allocation_group);
}

void kan_rpl_meta_parameter_shutdown (struct kan_rpl_meta_parameter_t *instance)
{
    kan_dynamic_array_shutdown (&instance->meta);
}

void kan_rpl_meta_buffer_init (struct kan_rpl_meta_buffer_t *instance)
{
    instance->name = NULL;
    instance->binding = 0u;
    instance->type = KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE;
    instance->size = 0u;
    kan_dynamic_array_init (&instance->attributes, 0u, sizeof (struct kan_rpl_meta_attribute_t),
                            _Alignof (struct kan_rpl_meta_attribute_t), rpl_meta_allocation_group);
    kan_dynamic_array_init (&instance->parameters, 0u, sizeof (struct kan_rpl_meta_parameter_t),
                            _Alignof (struct kan_rpl_meta_parameter_t), rpl_meta_allocation_group);
}

void kan_rpl_meta_buffer_shutdown (struct kan_rpl_meta_buffer_t *instance)
{
    for (uint64_t parameter_index = 0u; parameter_index < instance->parameters.size; ++parameter_index)
    {
        kan_rpl_meta_parameter_shutdown (
            &((struct kan_rpl_meta_parameter_t *) instance->parameters.data)[parameter_index]);
    }

    kan_dynamic_array_shutdown (&instance->attributes);
    kan_dynamic_array_shutdown (&instance->parameters);
}

void kan_rpl_meta_init (struct kan_rpl_meta_t *instance)
{
    ensure_statics_initialized ();
    instance->pipeline_type = KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC;
    instance->graphics_classic_settings = (struct kan_rpl_graphics_classic_pipeline_settings_t) {
        .polygon_mode = KAN_RPL_POLYGON_MODE_FILL,
        .cull_mode = KAN_RPL_CULL_MODE_BACK,
        .depth_test = KAN_TRUE,
        .depth_write = KAN_TRUE,
    };

    kan_dynamic_array_init (&instance->buffers, 0u, sizeof (struct kan_rpl_meta_buffer_t),
                            _Alignof (struct kan_rpl_meta_buffer_t), rpl_meta_allocation_group);
    kan_dynamic_array_init (&instance->samplers, 0u, sizeof (struct kan_rpl_meta_sampler_t),
                            _Alignof (struct kan_rpl_meta_sampler_t), rpl_meta_allocation_group);
}

void kan_rpl_meta_shutdown (struct kan_rpl_meta_t *instance)
{
    for (uint64_t index = 0u; index < instance->buffers.size; ++index)
    {
        kan_rpl_meta_buffer_shutdown (&((struct kan_rpl_meta_buffer_t *) instance->buffers.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->buffers);
    kan_dynamic_array_shutdown (&instance->samplers);
}

kan_rpl_parser_t kan_rpl_parser_create (kan_interned_string_t log_name)
{
    ensure_statics_initialized ();
    struct rpl_parser_t *parser = kan_allocate_general (rpl_parser_allocation_group, sizeof (struct rpl_parser_t),
                                                        _Alignof (struct rpl_parser_t));
    parser->log_name = log_name;
    parser_processing_data_init (&parser->processing_data);
    return (uint64_t) parser;
}

kan_bool_t kan_rpl_parser_add_source (kan_rpl_parser_t parser, const char *source, kan_interned_string_t log_name)
{
    struct rpl_parser_t *instance = (struct rpl_parser_t *) parser;
    struct dynamic_parser_state_t dynamic_state = {
        .source_log_name = log_name,
        .detached_conditional = NULL,
        .limit = source + strlen (source),
        .cursor = source,
        .marker = source,
        .token = source,
        .cursor_line = 1u,
        .cursor_symbol = 0u,
        .marker_line = 1u,
        .marker_symbol = 0u,
        .saved = source,
        .saved_line = 1u,
        .saved_symbol = 0u,
    };

    return parse_main (instance, &dynamic_state);
}

static kan_bool_t build_intermediate_options (struct rpl_parser_t *instance, struct kan_rpl_intermediate_t *output)
{
    kan_dynamic_array_set_capacity (&output->options, instance->processing_data.options.items.size);
    struct parser_option_t *source_option = (struct parser_option_t *) instance->processing_data.options.items.first;

    while (source_option)
    {
        struct kan_rpl_option_t *target_option = kan_dynamic_array_add_last (&output->options);
        KAN_ASSERT (target_option)

        target_option->name = source_option->name;
        target_option->scope = source_option->scope;
        target_option->type = source_option->type;

        switch (source_option->type)
        {
        case KAN_RPL_OPTION_TYPE_FLAG:
            target_option->flag_default_value = source_option->flag_default_value;
            break;

        case KAN_RPL_OPTION_TYPE_COUNT:
            target_option->count_default_value = source_option->count_default_value;
            break;
        }

        source_option = (struct parser_option_t *) source_option->node.list_node.next;
    }

    return KAN_TRUE;
}

static kan_bool_t build_intermediate_expression (struct rpl_parser_t *instance,
                                                 struct parser_expression_tree_node_t *expression,
                                                 struct kan_rpl_expression_node_t *output)
{
    kan_bool_t result = KAN_TRUE;
    output->type = expression->type;
    output->source_name = expression->source_log_name;
    output->source_line = (uint32_t) expression->source_line;

#define BUILD_SUB_EXPRESSION(NAME, INDEX, SOURCE)                                                                      \
    struct kan_rpl_expression_node_t *NAME = &((struct kan_rpl_expression_node_t *) output->children.data)[INDEX];     \
    kan_rpl_expression_node_init (NAME);                                                                               \
                                                                                                                       \
    if (!build_intermediate_expression (instance, SOURCE, NAME))                                                       \
    {                                                                                                                  \
        result = KAN_FALSE;                                                                                            \
    }

#define COLLECT_LIST_SIZE(NAME, LIST)                                                                                  \
    uint64_t NAME##_count = 0u;                                                                                        \
    struct parser_expression_list_item_t *NAME##_list = LIST;                                                          \
                                                                                                                       \
    while (NAME##_list)                                                                                                \
    {                                                                                                                  \
        ++NAME##_count;                                                                                                \
        NAME##_list = NAME##_list->next;                                                                               \
    }

#define BUILD_SUB_LIST(OFFSET, COUNT, LIST)                                                                            \
    struct parser_expression_list_item_t *sub_list = LIST;                                                             \
    for (uint64_t index = OFFSET; index < OFFSET + COUNT; ++index, sub_list = sub_list->next)                          \
    {                                                                                                                  \
        struct kan_rpl_expression_node_t *node = &((struct kan_rpl_expression_node_t *) output->children.data)[index]; \
        kan_rpl_expression_node_init (node);                                                                           \
                                                                                                                       \
        if (!build_intermediate_expression (instance, sub_list->expression, node))                                     \
        {                                                                                                              \
            result = KAN_FALSE;                                                                                        \
        }                                                                                                              \
    }

    switch (expression->type)
    {
    case KAN_RPL_EXPRESSION_NODE_TYPE_NOPE:
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER:
        output->identifier = expression->identifier;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_INTEGER_LITERAL:
        output->integer_literal = expression->integer_literal;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL:
        output->floating_literal = expression->floating_literal;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION:
    {
        output->variable_declaration.type_name = expression->variable_declaration.type;
        output->variable_declaration.variable_name = expression->variable_declaration.name;

        COLLECT_LIST_SIZE (dimension, expression->variable_declaration.array_size_list)
        kan_dynamic_array_set_capacity (&output->children, dimension_count);
        output->children.size = dimension_count;
        BUILD_SUB_LIST (0u, dimension_count, expression->variable_declaration.array_size_list)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION:
    {
        output->binary_operation = expression->binary_operation.binary_operation;
        kan_dynamic_array_set_capacity (&output->children, 2u);
        output->children.size = 2u;
        BUILD_SUB_EXPRESSION (left_node, 0u, expression->binary_operation.left_operand_expression)
        BUILD_SUB_EXPRESSION (right_node, 1u, expression->binary_operation.right_operand_expression)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION:
    {
        output->unary_operation = expression->unary_operation.unary_operation;
        kan_dynamic_array_set_capacity (&output->children, 1u);
        output->children.size = 1u;
        BUILD_SUB_EXPRESSION (node, 0u, expression->unary_operation.operand_expression)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE:
    {
        COLLECT_LIST_SIZE (expression, expression->scope_expressions_list)
        kan_dynamic_array_set_capacity (&output->children, expression_count);
        output->children.size = expression_count;
        BUILD_SUB_LIST (0u, expression_count, expression->scope_expressions_list)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL:
    {
        output->function_name = expression->function_call.function_name;
        COLLECT_LIST_SIZE (argument, expression->function_call.arguments)
        kan_dynamic_array_set_capacity (&output->children, argument_count);
        output->children.size = argument_count;
        BUILD_SUB_LIST (0u, argument_count, expression->function_call.arguments)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR:
    {
        output->constructor_type_name = expression->constructor.constructor_type_name;
        COLLECT_LIST_SIZE (argument, expression->constructor.arguments)
        kan_dynamic_array_set_capacity (&output->children, argument_count);
        output->children.size = argument_count;
        BUILD_SUB_LIST (0u, argument_count, expression->constructor.arguments)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_IF:
    {
        kan_dynamic_array_set_capacity (&output->children, expression->if_.false_expression ? 3u : 2u);
        output->children.size = expression->if_.false_expression ? 3u : 2u;
        BUILD_SUB_EXPRESSION (condition_node, 0u, expression->if_.condition_expression)
        BUILD_SUB_EXPRESSION (true_node, 1u, expression->if_.true_expression)

        if (expression->if_.false_expression)
        {
            BUILD_SUB_EXPRESSION (false_node, 2u, expression->if_.false_expression)
        }

        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FOR:
    {
        kan_dynamic_array_set_capacity (&output->children, 4u);
        output->children.size = 4u;
        BUILD_SUB_EXPRESSION (init_node, 0u, expression->for_.init_expression)
        BUILD_SUB_EXPRESSION (condition_node, 0u, expression->for_.condition_expression)
        BUILD_SUB_EXPRESSION (step_node, 0u, expression->for_.step_expression)
        BUILD_SUB_EXPRESSION (body_node, 0u, expression->for_.body_expression)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_WHILE:
    {
        kan_dynamic_array_set_capacity (&output->children, 2u);
        output->children.size = 2u;
        BUILD_SUB_EXPRESSION (condition_node, 0u, expression->while_.condition_expression)
        BUILD_SUB_EXPRESSION (body_node, 1u, expression->while_.body_expression)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE:
    {
        kan_dynamic_array_set_capacity (&output->children, 2u);
        output->children.size = 2u;
        BUILD_SUB_EXPRESSION (condition_node, 0u, expression->conditional_scope.condition_expression)
        BUILD_SUB_EXPRESSION (body_node, 1u, expression->conditional_scope.body_expression)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS:
    {
        output->alias_name = expression->conditional_alias.identifier;
        kan_dynamic_array_set_capacity (&output->children, 2u);
        output->children.size = 2u;
        BUILD_SUB_EXPRESSION (condition_node, 0u, expression->conditional_alias.condition_expression)
        BUILD_SUB_EXPRESSION (body_node, 1u, expression->conditional_alias.body_expression)
        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_BREAK:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONTINUE:
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_RETURN:
    {
        if (expression->return_expression)
        {
            kan_dynamic_array_set_capacity (&output->children, 1u);
            output->children.size = 1u;
            BUILD_SUB_EXPRESSION (node, 0u, expression->return_expression)
        }

        break;
    }
    }

#undef BUILD_SUB_EXPRESSION
#undef COLLECT_LIST_SIZE
#undef BUILD_SUB_LIST

    return result;
}

static kan_bool_t build_intermediate_setting (struct rpl_parser_t *instance,
                                              struct parser_setting_data_t *setting,
                                              struct kan_rpl_setting_t *output)
{
    output->name = setting->name;
    output->type = setting->type;
    output->source_name = setting->source_log_name;
    output->source_line = setting->source_line;

    switch (setting->type)
    {
    case KAN_RPL_SETTING_TYPE_FLAG:
        output->flag = setting->flag;
        break;

    case KAN_RPL_SETTING_TYPE_INTEGER:
        output->integer = setting->integer;
        break;

    case KAN_RPL_SETTING_TYPE_FLOATING:
        output->floating = setting->floating;
        break;

    case KAN_RPL_SETTING_TYPE_STRING:
        output->string = setting->string;
        break;
    }

    if (setting->conditional)
    {
        if (!build_intermediate_expression (instance, setting->conditional, &output->conditional))
        {
            return KAN_FALSE;
        }
    }

    return KAN_TRUE;
}

static kan_bool_t build_intermediate_settings (struct rpl_parser_t *instance, struct kan_rpl_intermediate_t *output)
{
    kan_dynamic_array_set_capacity (&output->settings, instance->processing_data.settings.items.size);
    struct parser_setting_t *source_setting =
        (struct parser_setting_t *) instance->processing_data.settings.items.first;
    kan_bool_t result = KAN_TRUE;

    while (source_setting)
    {
        struct kan_rpl_setting_t *target_setting = kan_dynamic_array_add_last (&output->settings);
        KAN_ASSERT (target_setting)
        kan_rpl_setting_init (target_setting);

        if (!build_intermediate_setting (instance, &source_setting->setting, target_setting))
        {
            result = KAN_FALSE;
        }

        source_setting = (struct parser_setting_t *) source_setting->node.list_node.next;
    }

    return result;
}

static kan_bool_t build_intermediate_declarations (struct rpl_parser_t *instance,
                                                   struct parser_declaration_t *first_declaration,
                                                   struct kan_dynamic_array_t *output)
{
    kan_bool_t result = KAN_TRUE;
    uint64_t count = 0u;
    struct parser_declaration_t *declaration = first_declaration;

    while (declaration)
    {
        ++count;
        declaration = declaration->next;
    }

    kan_dynamic_array_set_capacity (output, count);
    declaration = first_declaration;

    while (declaration)
    {
        struct kan_rpl_declaration_t *new_declaration = kan_dynamic_array_add_last (output);
        KAN_ASSERT (new_declaration)

        kan_rpl_declaration_init (new_declaration);
        new_declaration->name = declaration->declaration.name;
        new_declaration->type_name = declaration->declaration.type;
        new_declaration->source_name = declaration->source_log_name;
        new_declaration->source_line = declaration->source_line;

        uint64_t array_sizes_count = 0u;
        struct parser_expression_list_item_t *array_size = declaration->declaration.array_size_list;

        while (array_size)
        {
            ++array_sizes_count;
            array_size = array_size->next;
        }

        kan_dynamic_array_set_capacity (&new_declaration->array_sizes, array_sizes_count);
        array_size = declaration->declaration.array_size_list;

        while (array_size)
        {
            struct kan_rpl_expression_node_t *new_expression =
                kan_dynamic_array_add_last (&new_declaration->array_sizes);
            KAN_ASSERT (new_expression)
            kan_rpl_expression_node_init (new_expression);

            if (!build_intermediate_expression (instance, array_size->expression, new_expression))
            {
                result = KAN_FALSE;
            }

            array_size = array_size->next;
        }

        uint64_t meta_count = 0u;
        struct parser_declaration_meta_item_t *meta_item = declaration->first_meta;

        while (meta_item)
        {
            ++meta_count;
            meta_item = meta_item->next;
        }

        kan_dynamic_array_set_capacity (&new_declaration->meta, meta_count);
        meta_item = declaration->first_meta;

        while (meta_item)
        {
            kan_interned_string_t *next_meta = kan_dynamic_array_add_last (&new_declaration->meta);
            KAN_ASSERT (next_meta)
            *next_meta = meta_item->meta;
            meta_item = meta_item->next;
        }

        if (declaration->conditional &&
            !build_intermediate_expression (instance, declaration->conditional, &new_declaration->conditional))
        {
            result = KAN_FALSE;
        }

        declaration = declaration->next;
    }

    return result;
}

static kan_bool_t build_intermediate_structs (struct rpl_parser_t *instance, struct kan_rpl_intermediate_t *output)
{
    kan_bool_t result = KAN_TRUE;
    kan_dynamic_array_set_capacity (&output->structs, instance->processing_data.structs.items.size);

    struct kan_stack_group_allocator_t temporary_allocator;
    kan_stack_group_allocator_init (&temporary_allocator, rpl_parser_temporary_allocation_group,
                                    KAN_RPL_PARSER_INTERMEDIATE_SORT_TEMPORARY_SIZE);

    struct parser_struct_t *struct_data = (struct parser_struct_t *) instance->processing_data.structs.items.first;
    while (struct_data)
    {
        struct_data->sort_dependencies_count = 0u;
        struct_data->sort_first_dependant = NULL;
        struct_data->sort_added = KAN_FALSE;
        struct_data = (struct parser_struct_t *) struct_data->node.list_node.next;
    }

    struct_data = (struct parser_struct_t *) instance->processing_data.structs.items.first;
    while (struct_data)
    {
        struct parser_declaration_t *declaration = struct_data->first_declaration;
        while (declaration)
        {
            if (declaration->declaration.type == struct_data->name)
            {
                KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s:%s] [%ld] Field \"%s\" has its own struct type.",
                         instance->log_name, declaration->source_log_name, declaration->source_line,
                         declaration->declaration.name)
                result = KAN_FALSE;
            }
            else
            {
                struct parser_struct_t *other_struct = rpl_parser_find_struct (instance, declaration->declaration.type);
                if (other_struct)
                {
                    ++struct_data->sort_dependencies_count;
                    struct parser_struct_sort_dependant_t *dependant = kan_stack_group_allocator_allocate (
                        &temporary_allocator, sizeof (struct parser_struct_sort_dependant_t),
                        _Alignof (struct parser_struct_sort_dependant_t));

                    dependant->dependant = struct_data;
                    dependant->next = other_struct->sort_first_dependant;
                    other_struct->sort_first_dependant = dependant;
                }
            }

            declaration = declaration->next;
        }

        struct_data = (struct parser_struct_t *) struct_data->node.list_node.next;
    }

    while (KAN_TRUE)
    {
        kan_bool_t all_zeros = KAN_TRUE;
        kan_bool_t any_added = KAN_FALSE;

        struct_data = (struct parser_struct_t *) instance->processing_data.structs.items.first;
        while (struct_data)
        {
            if (struct_data->sort_dependencies_count == 0u)
            {
                if (!struct_data->sort_added)
                {
                    struct kan_rpl_struct_t *new_struct = kan_dynamic_array_add_last (&output->structs);
                    KAN_ASSERT (new_struct)

                    kan_rpl_struct_init (new_struct);
                    new_struct->name = struct_data->name;
                    new_struct->source_name = struct_data->source_log_name;
                    new_struct->source_line = struct_data->source_line;

                    if (!build_intermediate_declarations (instance, struct_data->first_declaration,
                                                          &new_struct->fields))
                    {
                        result = KAN_FALSE;
                    }

                    if (struct_data->conditional &&
                        !build_intermediate_expression (instance, struct_data->conditional, &new_struct->conditional))
                    {
                        result = KAN_FALSE;
                    }

                    struct parser_struct_sort_dependant_t *dependant = struct_data->sort_first_dependant;
                    while (dependant)
                    {
                        --dependant->dependant->sort_dependencies_count;
                        dependant = dependant->next;
                    }

                    struct_data->sort_added = KAN_TRUE;
                    any_added = KAN_TRUE;
                }
            }
            else
            {
                all_zeros = KAN_FALSE;
            }

            struct_data = (struct parser_struct_t *) struct_data->node.list_node.next;
        }

        if (all_zeros)
        {
            break;
        }

        if (!any_added)
        {
            KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                     "[%s] Unable to sort structs due to cycle. Dumping structs that aren't added.", instance->log_name)
            struct_data = (struct parser_struct_t *) instance->processing_data.structs.items.first;

            while (struct_data)
            {
                if (struct_data->sort_dependencies_count > 0u)
                {
                    KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s] Unable to sort struct \"%s\".", instance->log_name,
                             struct_data->name)
                }

                struct_data = (struct parser_struct_t *) struct_data->node.list_node.next;
            }

            result = KAN_FALSE;
            break;
        }
    }

    kan_stack_group_allocator_shutdown (&temporary_allocator);
    return result;
}

static kan_bool_t build_intermediate_buffers (struct rpl_parser_t *instance, struct kan_rpl_intermediate_t *output)
{
    kan_bool_t result = KAN_TRUE;
    kan_dynamic_array_set_capacity (&output->buffers, instance->processing_data.buffers.items.size);
    struct parser_buffer_t *source_buffer = (struct parser_buffer_t *) instance->processing_data.buffers.items.first;

    while (source_buffer)
    {
        struct kan_rpl_buffer_t *target_buffer = kan_dynamic_array_add_last (&output->buffers);
        KAN_ASSERT (target_buffer)

        kan_rpl_buffer_init (target_buffer);
        target_buffer->name = source_buffer->name;
        target_buffer->type = source_buffer->type;
        target_buffer->source_name = source_buffer->source_log_name;
        target_buffer->source_line = source_buffer->source_line;

        if (!build_intermediate_declarations (instance, source_buffer->first_declaration, &target_buffer->fields))
        {
            result = KAN_FALSE;
        }

        if (source_buffer->conditional &&
            !build_intermediate_expression (instance, source_buffer->conditional, &target_buffer->conditional))
        {
            result = KAN_FALSE;
        }

        source_buffer = (struct parser_buffer_t *) source_buffer->node.list_node.next;
    }

    return result;
}

static kan_bool_t build_intermediate_samplers (struct rpl_parser_t *instance, struct kan_rpl_intermediate_t *output)
{
    kan_bool_t result = KAN_TRUE;
    kan_dynamic_array_set_capacity (&output->samplers, instance->processing_data.samplers.items.size);
    struct parser_sampler_t *source_sampler =
        (struct parser_sampler_t *) instance->processing_data.samplers.items.first;

    while (source_sampler)
    {
        struct kan_rpl_sampler_t *target_sampler = kan_dynamic_array_add_last (&output->samplers);
        KAN_ASSERT (target_sampler)

        kan_rpl_sampler_init (target_sampler);
        target_sampler->name = source_sampler->name;
        target_sampler->type = source_sampler->type;
        target_sampler->source_name = source_sampler->source_log_name;
        target_sampler->source_line = source_sampler->source_line;

        uint64_t settings_count = 0u;
        struct parser_setting_list_item_t *setting = source_sampler->first_setting;

        while (setting)
        {
            ++settings_count;
            setting = setting->next;
        }

        kan_dynamic_array_set_capacity (&target_sampler->settings, settings_count);
        setting = source_sampler->first_setting;

        while (setting)
        {
            struct kan_rpl_setting_t *new_setting = kan_dynamic_array_add_last (&target_sampler->settings);
            KAN_ASSERT (new_setting)
            kan_rpl_setting_init (new_setting);

            if (!build_intermediate_setting (instance, &setting->setting, new_setting))
            {
                result = KAN_FALSE;
            }

            setting = setting->next;
        }

        if (source_sampler->conditional &&
            !build_intermediate_expression (instance, source_sampler->conditional, &target_sampler->conditional))
        {
            result = KAN_FALSE;
        }

        source_sampler = (struct parser_sampler_t *) source_sampler->node.list_node.next;
    }

    return result;
}

static void build_intermediate_function_collect_calls (struct rpl_parser_t *instance,
                                                       struct parser_function_t *function,
                                                       struct kan_stack_group_allocator_t *temporary_allocator,
                                                       struct parser_expression_tree_node_t *expression)
{
    switch (expression->type)
    {
    case KAN_RPL_EXPRESSION_NODE_TYPE_NOPE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER:
    case KAN_RPL_EXPRESSION_NODE_TYPE_INTEGER_LITERAL:
    case KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL:
    case KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR:
    case KAN_RPL_EXPRESSION_NODE_TYPE_BREAK:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONTINUE:
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION:
        build_intermediate_function_collect_calls (instance, function, temporary_allocator,
                                                   expression->binary_operation.left_operand_expression);
        build_intermediate_function_collect_calls (instance, function, temporary_allocator,
                                                   expression->binary_operation.right_operand_expression);
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION:
        build_intermediate_function_collect_calls (instance, function, temporary_allocator,
                                                   expression->unary_operation.operand_expression);
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE:
    {
        struct parser_expression_list_item_t *item = expression->scope_expressions_list;
        while (item)
        {
            build_intermediate_function_collect_calls (instance, function, temporary_allocator, item->expression);
            item = item->next;
        }

        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL:
    {
        if (expression->function_call.function_name != function->name)
        {
            struct parser_function_t *other_function =
                rpl_parser_find_function (instance, expression->function_call.function_name);

            if (other_function)
            {
                ++function->sort_dependencies_count;
                struct parser_function_sort_dependant_t *dependant = kan_stack_group_allocator_allocate (
                    temporary_allocator, sizeof (struct parser_function_sort_dependant_t),
                    _Alignof (struct parser_function_sort_dependant_t));

                dependant->dependant = function;
                dependant->next = other_function->sort_first_dependant;
                other_function->sort_first_dependant = dependant;
            }
        }

        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_IF:
        build_intermediate_function_collect_calls (instance, function, temporary_allocator,
                                                   expression->if_.condition_expression);
        build_intermediate_function_collect_calls (instance, function, temporary_allocator,
                                                   expression->if_.true_expression);

        if (expression->if_.false_expression)
        {
            build_intermediate_function_collect_calls (instance, function, temporary_allocator,
                                                       expression->if_.false_expression);
        }

        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FOR:
        build_intermediate_function_collect_calls (instance, function, temporary_allocator,
                                                   expression->for_.init_expression);
        build_intermediate_function_collect_calls (instance, function, temporary_allocator,
                                                   expression->for_.condition_expression);
        build_intermediate_function_collect_calls (instance, function, temporary_allocator,
                                                   expression->for_.step_expression);
        build_intermediate_function_collect_calls (instance, function, temporary_allocator,
                                                   expression->for_.body_expression);
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_WHILE:
        build_intermediate_function_collect_calls (instance, function, temporary_allocator,
                                                   expression->while_.condition_expression);
        build_intermediate_function_collect_calls (instance, function, temporary_allocator,
                                                   expression->while_.body_expression);
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE:
        build_intermediate_function_collect_calls (instance, function, temporary_allocator,
                                                   expression->conditional_scope.body_expression);
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS:
        build_intermediate_function_collect_calls (instance, function, temporary_allocator,
                                                   expression->conditional_alias.body_expression);
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_RETURN:
        build_intermediate_function_collect_calls (instance, function, temporary_allocator,
                                                   expression->return_expression);
        break;
    }
}

static kan_bool_t build_intermediate_functions (struct rpl_parser_t *instance, struct kan_rpl_intermediate_t *output)
{
    kan_bool_t result = KAN_TRUE;
    kan_dynamic_array_set_capacity (&output->functions, instance->processing_data.functions.items.size);

    struct kan_stack_group_allocator_t temporary_allocator;
    kan_stack_group_allocator_init (&temporary_allocator, rpl_parser_temporary_allocation_group,
                                    KAN_RPL_PARSER_INTERMEDIATE_SORT_TEMPORARY_SIZE);

    struct parser_function_t *function = (struct parser_function_t *) instance->processing_data.functions.items.first;
    while (function)
    {
        function->sort_dependencies_count = 0u;
        function->sort_first_dependant = NULL;
        function->sort_added = KAN_FALSE;
        function = (struct parser_function_t *) function->node.list_node.next;
    }

    function = (struct parser_function_t *) instance->processing_data.functions.items.first;
    while (function)
    {
        build_intermediate_function_collect_calls (instance, function, &temporary_allocator, function->body_expression);
        function = (struct parser_function_t *) function->node.list_node.next;
    }

    while (KAN_TRUE)
    {
        kan_bool_t all_zeros = KAN_TRUE;
        kan_bool_t any_added = KAN_FALSE;

        function = (struct parser_function_t *) instance->processing_data.functions.items.first;
        while (function)
        {
            if (function->sort_dependencies_count == 0u)
            {
                if (!function->sort_added)
                {
                    struct kan_rpl_function_t *new_function = kan_dynamic_array_add_last (&output->functions);
                    KAN_ASSERT (new_function)

                    kan_rpl_function_init (new_function);
                    new_function->return_type_name = function->return_type_name;
                    new_function->name = function->name;
                    new_function->source_name = function->source_log_name;
                    new_function->source_line = function->source_line;

                    // Special case -- void function.
                    if (function->first_argument->declaration.type != interned_void)
                    {
                        if (!build_intermediate_declarations (instance, function->first_argument,
                                                              &new_function->arguments))
                        {
                            result = KAN_FALSE;
                        }
                    }

                    if (!build_intermediate_expression (instance, function->body_expression, &new_function->body))
                    {
                        result = KAN_FALSE;
                    }

                    if (function->conditional &&
                        !build_intermediate_expression (instance, function->conditional, &new_function->conditional))
                    {
                        result = KAN_FALSE;
                    }

                    struct parser_function_sort_dependant_t *dependant = function->sort_first_dependant;
                    while (dependant)
                    {
                        --dependant->dependant->sort_dependencies_count;
                        dependant = dependant->next;
                    }

                    function->sort_added = KAN_TRUE;
                    any_added = KAN_TRUE;
                }
            }
            else
            {
                all_zeros = KAN_FALSE;
            }

            function = (struct parser_function_t *) function->node.list_node.next;
        }

        if (all_zeros)
        {
            break;
        }

        if (!any_added)
        {
            KAN_LOG (rpl_parser, KAN_LOG_ERROR,
                     "[%s] Unable to sort functions due to cycle. Dumping functions that aren't added.",
                     instance->log_name)
            function = (struct parser_function_t *) instance->processing_data.functions.items.first;

            while (function)
            {
                if (function->sort_dependencies_count > 0u)
                {
                    KAN_LOG (rpl_parser, KAN_LOG_ERROR, "[%s] Unable to sort function \"%s\".", instance->log_name,
                             function->name)
                }

                function = (struct parser_function_t *) function->node.list_node.next;
            }

            result = KAN_FALSE;
            break;
        }
    }

    kan_stack_group_allocator_shutdown (&temporary_allocator);
    return result;
}

kan_bool_t kan_rpl_parser_build_intermediate (kan_rpl_parser_t parser, struct kan_rpl_intermediate_t *output)
{
    struct rpl_parser_t *instance = (struct rpl_parser_t *) parser;
    output->log_name = instance->log_name;
    return build_intermediate_options (instance, output) && build_intermediate_settings (instance, output) &&
           build_intermediate_structs (instance, output) && build_intermediate_buffers (instance, output) &&
           build_intermediate_samplers (instance, output) && build_intermediate_functions (instance, output);
}

void kan_rpl_parser_destroy (kan_rpl_parser_t parser)
{
    struct rpl_parser_t *instance = (struct rpl_parser_t *) parser;
    parser_processing_data_shutdown (&instance->processing_data);
    kan_free_general (rpl_parser_allocation_group, instance, sizeof (struct rpl_parser_t));
}

kan_allocation_group_t kan_rpl_get_emission_allocation_group (void)
{
    ensure_statics_initialized ();
    return rpl_emission_allocation_group;
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
