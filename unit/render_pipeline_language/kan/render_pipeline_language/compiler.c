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
#include <kan/render_pipeline_language/compiler.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (rpl_compiler_context);
KAN_LOG_DEFINE_CATEGORY (rpl_compiler_instance);

struct rpl_compiler_context_option_value_t
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

struct rpl_compiler_context_t
{
    enum kan_rpl_pipeline_type_t pipeline_type;
    kan_interned_string_t log_name;

    /// \meta reflection_dynamic_array_type = "struct rpl_compiler_context_option_value_t"
    struct kan_dynamic_array_t option_values;

    /// \meta reflection_dynamic_array_type = "struct kan_rpl_intermediate_t *"
    struct kan_dynamic_array_t modules;

    struct kan_stack_group_allocator_t resolve_allocator;
};

struct compiler_instance_setting_node_t
{
    struct compiler_instance_setting_node_t *next;
    kan_interned_string_t name;
    enum kan_rpl_setting_type_t type;

    union
    {
        kan_bool_t flag;
        int64_t integer;
        double floating;
        kan_interned_string_t string;
    };

    /// \details Conditional expression if it is not KAN_RPL_EXPRESSION_NODE_TYPE_NOPE.
    struct kan_rpl_expression_node_t conditional;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    uint32_t source_line;
};

struct compiler_instance_variable_t
{
    kan_interned_string_t name;
    struct inbuilt_vector_type_t *type_if_vector;
    struct inbuilt_matrix_type_t *type_if_matrix;
    struct compiler_instance_struct_node_t *type_if_struct;

    uint64_t array_dimensions_count;
    uint64_t *array_dimensions;
};

struct compiler_instance_declaration_node_t
{
    struct compiler_instance_declaration_node_t *next;
    struct compiler_instance_variable_t variable;

    uint64_t offset;
    uint64_t size;
    uint64_t alignment;

    uint64_t meta_count;
    kan_interned_string_t *meta;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    uint32_t source_line;
};

struct compiler_instance_struct_node_t
{
    struct compiler_instance_struct_node_t *next;
    kan_interned_string_t name;
    uint64_t size;
    uint64_t alignment;
    struct compiler_instance_declaration_node_t *first_field;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    uint32_t source_line;
};

struct flattening_name_generation_buffer_t
{
    uint64_t length;
    char buffer[KAN_RPL_COMPILER_INSTANCE_MAX_FLAT_NAME_LENGTH];
};

#define INVALID_LOCATION UINT64_MAX
#define INVALID_BINDING UINT64_MAX

struct binding_location_assignment_counter_t
{
    uint64_t next_attribute_buffer_binding;
    uint64_t next_arbitrary_buffer_binding;
    uint64_t next_attribute_location;
    uint64_t next_vertex_output_location;
    uint64_t next_fragment_output_location;
};

struct compiler_instance_buffer_flattened_declaration_t
{
    struct compiler_instance_buffer_flattened_declaration_t *next;
    struct compiler_instance_declaration_node_t *source_declaration;
    kan_interned_string_t readable_name;
    uint64_t location;
};

struct compiler_instance_buffer_flattening_graph_node_t
{
    struct compiler_instance_buffer_flattening_graph_node_t *next_on_level;
    struct compiler_instance_buffer_flattening_graph_node_t *first_child;
    kan_interned_string_t name;
    struct compiler_instance_buffer_flattened_declaration_t *flattened_result;
};

struct compiler_instance_buffer_node_t
{
    struct compiler_instance_buffer_node_t *next;
    kan_interned_string_t name;
    enum kan_rpl_buffer_type_t type;
    kan_bool_t used;

    uint64_t size;
    uint64_t alignment;
    struct compiler_instance_declaration_node_t *first_field;

    uint64_t binding;
    struct compiler_instance_buffer_flattening_graph_node_t *flattening_graph_base;
    struct compiler_instance_buffer_flattened_declaration_t *first_flattened_declaration;
    struct compiler_instance_buffer_flattened_declaration_t *last_flattened_declaration;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    uint32_t source_line;
};

struct compiler_instance_sampler_node_t
{
    struct compiler_instance_sampler_node_t *next;
    kan_interned_string_t name;
    enum kan_rpl_sampler_type_t type;
    kan_bool_t used;

    uint64_t binding;
    struct compiler_instance_setting_node_t *first_setting;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    uint32_t source_line;
};

enum compiler_instance_expression_type_t
{
    COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_REFERENCE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS,
    COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS,
    COMPILER_INSTANCE_EXPRESSION_TYPE_INTEGER_LITERAL,
    COMPILER_INSTANCE_EXPRESSION_TYPE_FLOATING_LITERAL,
    COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_DECLARATION,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ARRAY_INDEX,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ADD,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_SUBTRACT,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_MULTIPLY,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_DIVIDE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_MODULUS,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ASSIGN,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_AND,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_OR,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_EQUAL,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NOT_EQUAL,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_LESS,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_GREATER,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_LESS_OR_EQUAL,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_GREATER_OR_EQUAL,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_AND,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_OR,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_XOR,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_LEFT_SHIFT,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_RIGHT_SHIFT,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NEGATE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NOT,
    COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_NOT,
    COMPILER_INSTANCE_EXPRESSION_TYPE_SCOPE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_FUNCTION_CALL,
    COMPILER_INSTANCE_EXPRESSION_TYPE_SAMPLER_CALL,
    COMPILER_INSTANCE_EXPRESSION_TYPE_CONSTRUCTOR,
    COMPILER_INSTANCE_EXPRESSION_TYPE_IF,
    COMPILER_INSTANCE_EXPRESSION_TYPE_FOR,
    COMPILER_INSTANCE_EXPRESSION_TYPE_WHILE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_BREAK,
    COMPILER_INSTANCE_EXPRESSION_TYPE_CONTINUE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_RETURN,
};

struct compiler_instance_structured_access_suffix_t
{
    struct compiler_instance_expression_node_t *input;
    uint64_t access_chain_length;
    uint64_t *access_chain_indices;
};

struct compiler_instance_binary_operation_suffix_t
{
    struct compiler_instance_expression_node_t *left_operand;
    struct inbuilt_vector_type_t *left_type_if_vector;
    struct inbuilt_matrix_type_t *left_type_if_matrix;
    struct compiler_instance_struct_node_t *left_type_if_struct;

    struct compiler_instance_expression_node_t *right_operand;
    struct inbuilt_vector_type_t *right_type_if_vector;
    struct inbuilt_matrix_type_t *right_type_if_matrix;
    struct compiler_instance_struct_node_t *right_type_if_struct;
};

struct compiler_instance_unary_operation_suffix_t
{
    struct compiler_instance_expression_node_t *operand;
    struct inbuilt_vector_type_t *type_if_vector;
    struct inbuilt_matrix_type_t *type_if_matrix;
    struct compiler_instance_struct_node_t *type_if_struct;
};

struct compiler_instance_expression_list_item_t
{
    struct compiler_instance_expression_list_item_t *next;
    struct compiler_instance_expression_node_t *expression;
};

struct compiler_instance_scope_variable_item_t
{
    struct compiler_instance_scope_variable_item_t *next;
    struct compiler_instance_variable_t *variable;
};

struct compiler_instance_scope_suffix_t
{
    struct compiler_instance_scope_variable_item_t *first_variable;
    struct compiler_instance_expression_list_item_t *first_expression;
};

struct compiler_instance_function_call_suffix_t
{
    struct compiler_instance_function_node_t *function;
    struct compiler_instance_expression_list_item_t *first_argument;
};

struct compiler_instance_sampler_call_suffix_t
{
    struct compiler_instance_sampler_node_t *sampler;
    struct compiler_instance_expression_list_item_t *first_argument;
};

struct compiler_instance_constructor_suffix_t
{
    struct inbuilt_vector_type_t *type_if_vector;
    struct inbuilt_matrix_type_t *type_if_matrix;
    struct compiler_instance_struct_node_t *type_if_struct;
    struct compiler_instance_expression_list_item_t *first_argument;
};

struct compiler_instance_if_suffix_t
{
    struct compiler_instance_expression_node_t *condition;
    struct compiler_instance_expression_node_t *when_true;
    struct compiler_instance_expression_node_t *when_false;
};

struct compiler_instance_for_suffix_t
{
    struct compiler_instance_expression_node_t *init;
    struct compiler_instance_expression_node_t *condition;
    struct compiler_instance_expression_node_t *step;
    struct compiler_instance_expression_node_t *body;
};

struct compiler_instance_while_suffix_t
{
    struct compiler_instance_expression_node_t *condition;
    struct compiler_instance_expression_node_t *body;
};

struct compiler_instance_expression_node_t
{
    enum compiler_instance_expression_type_t type;
    union
    {
        struct compiler_instance_buffer_node_t *structured_buffer_reference;
        struct compiler_instance_scope_variable_item_t *variable_reference;
        struct compiler_instance_structured_access_suffix_t structured_access;
        struct compiler_instance_buffer_flattened_declaration_t *flattened_buffer_access;
        int64_t integer_literal;
        double floating_literal;
        struct compiler_instance_variable_t variable_declaration;
        struct compiler_instance_binary_operation_suffix_t binary_operation;
        struct compiler_instance_unary_operation_suffix_t unary_operation;
        struct compiler_instance_scope_suffix_t scope;
        struct compiler_instance_function_call_suffix_t function_call;
        struct compiler_instance_sampler_call_suffix_t sampler_call;
        struct compiler_instance_constructor_suffix_t constructor;
        struct compiler_instance_if_suffix_t if_;
        struct compiler_instance_for_suffix_t for_;
        struct compiler_instance_while_suffix_t while_;
        struct compiler_instance_expression_node_t *break_loop;
        struct compiler_instance_expression_node_t *continue_loop;
        struct compiler_instance_expression_node_t *return_expression;
    };

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    uint32_t source_line;
};

struct compiler_instance_buffer_access_node_t
{
    struct compiler_instance_buffer_access_node_t *next;
    struct compiler_instance_buffer_node_t *buffer;
    struct compiler_instance_function_node_t *direct_access_function;
};

struct compiler_instance_sampler_access_node_t
{
    struct compiler_instance_sampler_access_node_t *next;
    struct compiler_instance_sampler_node_t *sampler;
    struct compiler_instance_function_node_t *direct_access_function;
};

struct compiler_instance_function_node_t
{
    struct compiler_instance_function_node_t *next;
    kan_interned_string_t name;
    struct inbuilt_vector_type_t *return_type_if_vector;
    struct inbuilt_matrix_type_t *return_type_if_matrix;
    struct compiler_instance_struct_node_t *return_type_if_struct;

    struct compiler_instance_declaration_node_t *first_argument;
    struct compiler_instance_expression_node_t *body;
    struct compiler_instance_scope_variable_item_t *first_argument_variable;

    kan_bool_t has_stage_specific_access;
    enum kan_rpl_pipeline_stage_t required_stage;
    struct compiler_instance_buffer_access_node_t *first_buffer_access;
    struct compiler_instance_sampler_access_node_t *first_sampler_access;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    uint32_t source_line;
};

struct rpl_compiler_instance_t
{
    enum kan_rpl_pipeline_type_t pipeline_type;
    kan_interned_string_t context_log_name;
    struct kan_stack_group_allocator_t resolve_allocator;

    uint64_t entry_point_count;
    struct kan_rpl_entry_point_t *entry_points;

    struct compiler_instance_setting_node_t *first_setting;
    struct compiler_instance_setting_node_t *last_setting;

    struct compiler_instance_struct_node_t *first_struct;
    struct compiler_instance_struct_node_t *last_struct;

    struct compiler_instance_buffer_node_t *first_buffer;
    struct compiler_instance_buffer_node_t *last_buffer;

    struct compiler_instance_sampler_node_t *first_sampler;
    struct compiler_instance_sampler_node_t *last_sampler;

    struct compiler_instance_function_node_t *first_function;
    struct compiler_instance_function_node_t *last_function;
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

struct resolve_expression_output_type_t
{
    struct inbuilt_vector_type_t *type_if_vector;
    struct inbuilt_matrix_type_t *type_if_matrix;
    struct compiler_instance_struct_node_t *type_if_struct;
    uint64_t array_dimensions_count;
    uint64_t *array_dimensions;
    kan_bool_t boolean;
    kan_bool_t writable;
};

struct resolve_expression_alias_node_t
{
    struct resolve_expression_alias_node_t *next;
    kan_interned_string_t name;

    // Compiler instance expressions do not have links to parents, therefore we can safely resolve alias once and
    // paste it as a link to every detected usage.
    struct compiler_instance_expression_node_t *resolved_expression;

    struct resolve_expression_output_type_t resolved_output_type;
};

struct resolve_expression_scope_t
{
    struct resolve_expression_scope_t *parent;
    struct compiler_instance_function_node_t *function;
    struct resolve_expression_alias_node_t *first_alias;
    struct compiler_instance_expression_node_t *associated_resolved_scope_if_any;
    struct compiler_instance_expression_node_t *associated_outer_loop_if_any;
};

struct resolve_fiend_access_linear_node_t
{
    struct resolve_fiend_access_linear_node_t *next;
    struct kan_rpl_expression_node_t *field_source;
};

enum inbuilt_type_item_t
{
    INBUILT_TYPE_ITEM_FLOAT = 0u,
    INBUILT_TYPE_ITEM_INTEGER,
};

static uint32_t inbuilt_type_item_size[] = {
    4u,
    4u,
};

struct inbuilt_vector_type_t
{
    kan_interned_string_t name;
    enum inbuilt_type_item_t item;
    uint32_t items_count;
    enum kan_rpl_meta_variable_type_t meta_type;
    struct compiler_instance_declaration_node_t *constructor_signature;

    uint32_t spirv_id;
};

struct inbuilt_matrix_type_t
{
    kan_interned_string_t name;
    enum inbuilt_type_item_t item;
    uint32_t rows;
    uint32_t columns;
    enum kan_rpl_meta_variable_type_t meta_type;
    struct compiler_instance_declaration_node_t *constructor_signature;

    uint32_t spirv_id;
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

static kan_bool_t statics_initialized = KAN_FALSE;
static kan_allocation_group_t rpl_allocation_group;
static kan_allocation_group_t rpl_meta_allocation_group;
static kan_allocation_group_t rpl_compiler_allocation_group;
static kan_allocation_group_t rpl_compiler_context_allocation_group;
static kan_allocation_group_t rpl_compiler_instance_allocation_group;

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

static struct inbuilt_vector_type_t type_f1;
static struct compiler_instance_declaration_node_t type_f1_constructor_signatures[1u];

static struct inbuilt_vector_type_t type_f2;
static struct compiler_instance_declaration_node_t type_f2_constructor_signatures[2u];

static struct inbuilt_vector_type_t type_f3;
static struct compiler_instance_declaration_node_t type_f3_constructor_signatures[3u];

static struct inbuilt_vector_type_t type_f4;
static struct compiler_instance_declaration_node_t type_f4_constructor_signatures[4u];

static struct inbuilt_vector_type_t type_i1;
static struct compiler_instance_declaration_node_t type_i1_constructor_signatures[1u];

static struct inbuilt_vector_type_t type_i2;
static struct compiler_instance_declaration_node_t type_i2_constructor_signatures[2u];

static struct inbuilt_vector_type_t type_i3;
static struct compiler_instance_declaration_node_t type_i3_constructor_signatures[3u];

static struct inbuilt_vector_type_t type_i4;
static struct compiler_instance_declaration_node_t type_i4_constructor_signatures[4u];

static struct inbuilt_vector_type_t *vector_types[] = {&type_f1, &type_f2, &type_f3, &type_f4,
                                                       &type_i1, &type_i2, &type_i3, &type_i4};
static struct inbuilt_vector_type_t *floating_vector_types[] = {&type_f1, &type_f2, &type_f3, &type_f4};
static struct inbuilt_vector_type_t *integer_vector_types[] = {&type_i1, &type_i2, &type_i3, &type_i4};

static struct inbuilt_matrix_type_t type_f3x3;
struct compiler_instance_declaration_node_t type_f3x3_constructor_signatures[3u];

static struct inbuilt_matrix_type_t type_f4x4;
struct compiler_instance_declaration_node_t type_f4x4_constructor_signatures[4u];

static struct inbuilt_matrix_type_t *matrix_types[] = {&type_f3x3, &type_f4x4};

static struct compiler_instance_declaration_node_t *sampler_2d_call_signature_first_element;
static struct compiler_instance_declaration_node_t sampler_2d_call_signature_location;

static struct compiler_instance_function_node_t *glsl_450_builtin_functions_first;
static struct compiler_instance_function_node_t glsl_450_sqrt;
static struct compiler_instance_declaration_node_t glsl_450_sqrt_arguments[1u];

static struct compiler_instance_function_node_t *shader_standard_builtin_functions_first;
static struct compiler_instance_function_node_t shader_standard_vertex_stage_output_position;
static struct compiler_instance_declaration_node_t shader_standard_vertex_stage_output_position_arguments[1u];

static struct compiler_instance_function_node_t shader_standard_i1_to_f1;
static struct compiler_instance_declaration_node_t shader_standard_i1_to_f1_arguments[1u];

static struct compiler_instance_function_node_t shader_standard_i2_to_f2;
static struct compiler_instance_declaration_node_t shader_standard_i2_to_f2_arguments[1u];

static struct compiler_instance_function_node_t shader_standard_i3_to_f3;
static struct compiler_instance_declaration_node_t shader_standard_i3_to_f3_arguments[1u];

static struct compiler_instance_function_node_t shader_standard_i4_to_f4;
static struct compiler_instance_declaration_node_t shader_standard_i4_to_f4_arguments[1u];

static void build_repeating_vector_constructor_signatures (
    struct inbuilt_vector_type_t *item,
    uint64_t repeats,
    struct compiler_instance_declaration_node_t *declaration_array_output)
{
    char name[3u] = "_0";
    for (uint64_t index = 0u; index < repeats; ++index)
    {
        name[1u] = (char) ('0' + index);
        declaration_array_output[index] = (struct compiler_instance_declaration_node_t) {
            .next = index + 1u == repeats ? NULL : &declaration_array_output[index + 1u],
            .variable =
                {
                    .name = kan_string_intern (name),
                    .type_if_vector = item,
                    .type_if_matrix = NULL,
                    .type_if_struct = NULL,
                    .array_dimensions_count = 0u,
                    .array_dimensions = NULL,
                },
            .meta_count = 0u,
            .meta = NULL,
            .module_name = NULL,
            .source_name = NULL,
            .source_line = 0u,
        };
    }
}

static inline void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        rpl_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "render_pipeline_language");
        rpl_meta_allocation_group = kan_allocation_group_get_child (rpl_allocation_group, "meta");
        rpl_compiler_allocation_group = kan_allocation_group_get_child (rpl_allocation_group, "compiler");
        rpl_compiler_context_allocation_group =
            kan_allocation_group_get_child (rpl_compiler_allocation_group, "context");
        rpl_compiler_instance_allocation_group =
            kan_allocation_group_get_child (rpl_compiler_allocation_group, "instance");

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
            .constructor_signature = type_f1_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_FLOAT,
        };

        type_f2 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("f2"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .items_count = 2u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F2,
            .constructor_signature = type_f2_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_F2,
        };

        type_f3 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("f3"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .items_count = 3u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F3,
            .constructor_signature = type_f3_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_F3,
        };

        type_f4 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("f4"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .items_count = 4u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F4,
            .constructor_signature = type_f4_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_F4,
        };

        type_i1 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("i1"),
            .item = INBUILT_TYPE_ITEM_INTEGER,
            .items_count = 1u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_I1,
            .constructor_signature = type_i1_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_INTEGER,
        };

        type_i2 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("i2"),
            .item = INBUILT_TYPE_ITEM_INTEGER,
            .items_count = 2u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_I2,
            .constructor_signature = type_i2_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_I2,
        };

        type_i3 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("i3"),
            .item = INBUILT_TYPE_ITEM_INTEGER,
            .items_count = 3u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_I3,
            .constructor_signature = type_i3_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_I3,
        };

        type_i4 = (struct inbuilt_vector_type_t) {
            .name = kan_string_intern ("i4"),
            .item = INBUILT_TYPE_ITEM_INTEGER,
            .items_count = 4u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_I4,
            .constructor_signature = type_i4_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_I4,
        };

        type_f3x3 = (struct inbuilt_matrix_type_t) {
            .name = kan_string_intern ("f3x3"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .rows = 3u,
            .columns = 3u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F3X3,
            .constructor_signature = type_f3x3_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_F3X3,
        };

        type_f4x4 = (struct inbuilt_matrix_type_t) {
            .name = kan_string_intern ("f4x4"),
            .item = INBUILT_TYPE_ITEM_FLOAT,
            .rows = 4u,
            .columns = 4u,
            .meta_type = KAN_RPL_META_VARIABLE_TYPE_F4X4,
            .constructor_signature = type_f4x4_constructor_signatures,
            .spirv_id = SPIRV_FIXED_ID_TYPE_F4X4,
        };

        build_repeating_vector_constructor_signatures (&type_f1, 1u, type_f1_constructor_signatures);
        build_repeating_vector_constructor_signatures (&type_f1, 2u, type_f2_constructor_signatures);
        build_repeating_vector_constructor_signatures (&type_f1, 3u, type_f3_constructor_signatures);
        build_repeating_vector_constructor_signatures (&type_f1, 4u, type_f4_constructor_signatures);
        build_repeating_vector_constructor_signatures (&type_i1, 1u, type_i1_constructor_signatures);
        build_repeating_vector_constructor_signatures (&type_i1, 2u, type_i2_constructor_signatures);
        build_repeating_vector_constructor_signatures (&type_i1, 3u, type_i3_constructor_signatures);
        build_repeating_vector_constructor_signatures (&type_i1, 4u, type_i4_constructor_signatures);
        build_repeating_vector_constructor_signatures (&type_f3, 3u, type_f3x3_constructor_signatures);
        build_repeating_vector_constructor_signatures (&type_f4, 4u, type_f4x4_constructor_signatures);

        const kan_interned_string_t interned_sampler = kan_string_intern ("sampler");
        const kan_interned_string_t interned_calls = kan_string_intern ("calls");

        sampler_2d_call_signature_first_element = &sampler_2d_call_signature_location;
        sampler_2d_call_signature_location = (struct compiler_instance_declaration_node_t) {
            .next = NULL,
            .variable =
                {
                    .name = kan_string_intern ("location"),
                    .type_if_vector = &type_f2,
                    .type_if_matrix = NULL,
                    .type_if_struct = NULL,
                    .array_dimensions_count = 0u,
                    .array_dimensions = NULL,
                },
            .meta_count = 0u,
            .meta = NULL,
            .module_name = interned_sampler,
            .source_name = interned_calls,
            .source_line = 0u,
        };

        glsl_450_builtin_functions_first = &glsl_450_sqrt;
        const kan_interned_string_t module_glsl_450 = kan_string_intern ("glsl_450_standard");
        const kan_interned_string_t source_functions = kan_string_intern ("functions");

        glsl_450_sqrt_arguments[0u] = (struct compiler_instance_declaration_node_t) {
            .next = NULL,
            .variable =
                {
                    .name = kan_string_intern ("number"),
                    .type_if_vector = &type_f1,
                    .type_if_matrix = NULL,
                    .type_if_struct = NULL,
                    .array_dimensions_count = 0u,
                    .array_dimensions = NULL,
                },
            .meta_count = 0u,
            .meta = NULL,
            .module_name = module_glsl_450,
            .source_name = source_functions,
            .source_line = 0u,
        };

        glsl_450_sqrt = (struct compiler_instance_function_node_t) {
            .next = NULL,
            .name = kan_string_intern ("sqrt"),
            .return_type_if_vector = &type_f1,
            .return_type_if_matrix = NULL,
            .return_type_if_struct = NULL,
            .first_argument = glsl_450_sqrt_arguments,
            .body = NULL,
            .has_stage_specific_access = KAN_FALSE,
            .required_stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX,
            .first_buffer_access = NULL,
            .first_sampler_access = NULL,
            .module_name = module_glsl_450,
            .source_name = source_functions,
            .source_line = 0u,
        };

        shader_standard_builtin_functions_first = &shader_standard_vertex_stage_output_position;
        const kan_interned_string_t module_shader_standard = kan_string_intern ("shader_standard");

        shader_standard_vertex_stage_output_position_arguments[0u] = (struct compiler_instance_declaration_node_t) {
            .next = NULL,
            .variable =
                {
                    .name = kan_string_intern ("position"),
                    .type_if_vector = &type_f4,
                    .type_if_matrix = NULL,
                    .type_if_struct = NULL,
                    .array_dimensions_count = 0u,
                    .array_dimensions = NULL,
                },
            .meta_count = 0u,
            .meta = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        shader_standard_vertex_stage_output_position = (struct compiler_instance_function_node_t) {
            .next = &shader_standard_i1_to_f1,
            .name = kan_string_intern ("vertex_stage_output_position"),
            .return_type_if_vector = NULL,
            .return_type_if_matrix = NULL,
            .return_type_if_struct = NULL,
            .first_argument = shader_standard_vertex_stage_output_position_arguments,
            .body = NULL,
            .has_stage_specific_access = KAN_TRUE,
            .required_stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX,
            .first_buffer_access = NULL,
            .first_sampler_access = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        shader_standard_i1_to_f1_arguments[0u] = (struct compiler_instance_declaration_node_t) {
            .next = NULL,
            .variable =
                {
                    .name = kan_string_intern ("value"),
                    .type_if_vector = &type_i1,
                    .type_if_matrix = NULL,
                    .type_if_struct = NULL,
                    .array_dimensions_count = 0u,
                    .array_dimensions = NULL,
                },
            .meta_count = 0u,
            .meta = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        shader_standard_i1_to_f1 = (struct compiler_instance_function_node_t) {
            .next = &shader_standard_i2_to_f2,
            .name = kan_string_intern ("i1_to_f1"),
            .return_type_if_vector = NULL,
            .return_type_if_matrix = NULL,
            .return_type_if_struct = NULL,
            .first_argument = shader_standard_i1_to_f1_arguments,
            .body = NULL,
            .has_stage_specific_access = KAN_TRUE,
            .required_stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX,
            .first_buffer_access = NULL,
            .first_sampler_access = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        shader_standard_i2_to_f2_arguments[0u] = (struct compiler_instance_declaration_node_t) {
            .next = NULL,
            .variable =
                {
                    .name = kan_string_intern ("value"),
                    .type_if_vector = &type_i2,
                    .type_if_matrix = NULL,
                    .type_if_struct = NULL,
                    .array_dimensions_count = 0u,
                    .array_dimensions = NULL,
                },
            .meta_count = 0u,
            .meta = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        shader_standard_i2_to_f2 = (struct compiler_instance_function_node_t) {
            .next = &shader_standard_i3_to_f3,
            .name = kan_string_intern ("i2_to_f2"),
            .return_type_if_vector = NULL,
            .return_type_if_matrix = NULL,
            .return_type_if_struct = NULL,
            .first_argument = shader_standard_i2_to_f2_arguments,
            .body = NULL,
            .has_stage_specific_access = KAN_TRUE,
            .required_stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX,
            .first_buffer_access = NULL,
            .first_sampler_access = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        shader_standard_i3_to_f3_arguments[0u] = (struct compiler_instance_declaration_node_t) {
            .next = NULL,
            .variable =
                {
                    .name = kan_string_intern ("value"),
                    .type_if_vector = &type_i3,
                    .type_if_matrix = NULL,
                    .type_if_struct = NULL,
                    .array_dimensions_count = 0u,
                    .array_dimensions = NULL,
                },
            .meta_count = 0u,
            .meta = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        shader_standard_i3_to_f3 = (struct compiler_instance_function_node_t) {
            .next = &shader_standard_i4_to_f4,
            .name = kan_string_intern ("i3_to_f3"),
            .return_type_if_vector = NULL,
            .return_type_if_matrix = NULL,
            .return_type_if_struct = NULL,
            .first_argument = shader_standard_i3_to_f3_arguments,
            .body = NULL,
            .has_stage_specific_access = KAN_TRUE,
            .required_stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX,
            .first_buffer_access = NULL,
            .first_sampler_access = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        shader_standard_i4_to_f4_arguments[0u] = (struct compiler_instance_declaration_node_t) {
            .next = NULL,
            .variable =
                {
                    .name = kan_string_intern ("value"),
                    .type_if_vector = &type_i4,
                    .type_if_matrix = NULL,
                    .type_if_struct = NULL,
                    .array_dimensions_count = 0u,
                    .array_dimensions = NULL,
                },
            .meta_count = 0u,
            .meta = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        shader_standard_i4_to_f4 = (struct compiler_instance_function_node_t) {
            .next = NULL,
            .name = kan_string_intern ("i4_to_f4"),
            .return_type_if_vector = NULL,
            .return_type_if_matrix = NULL,
            .return_type_if_struct = NULL,
            .first_argument = shader_standard_i4_to_f4_arguments,
            .body = NULL,
            .has_stage_specific_access = KAN_TRUE,
            .required_stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX,
            .first_buffer_access = NULL,
            .first_sampler_access = NULL,
            .module_name = module_shader_standard,
            .source_name = source_functions,
            .source_line = 0u,
        };

        statics_initialized = KAN_TRUE;
    }
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

kan_rpl_compiler_context_t kan_rpl_compiler_context_create (enum kan_rpl_pipeline_type_t pipeline_type,
                                                            kan_interned_string_t log_name)
{
    ensure_statics_initialized ();
    struct rpl_compiler_context_t *instance =
        kan_allocate_general (rpl_compiler_context_allocation_group, sizeof (struct rpl_compiler_context_t),
                              _Alignof (struct rpl_compiler_context_t));

    instance->pipeline_type = pipeline_type;
    instance->log_name = log_name;

    kan_dynamic_array_init (&instance->option_values, 0u, sizeof (struct rpl_compiler_context_option_value_t),
                            _Alignof (struct rpl_compiler_context_option_value_t), rpl_compiler_allocation_group);
    kan_dynamic_array_init (&instance->modules, 0u, sizeof (struct kan_rpl_intermediate_t *),
                            _Alignof (struct kan_rpl_intermediate_t *), rpl_compiler_allocation_group);
    kan_stack_group_allocator_init (&instance->resolve_allocator, rpl_compiler_context_allocation_group,
                                    KAN_RPL_COMPILER_CONTEXT_RESOLVE_STACK);

    return (kan_rpl_compiler_context_t) instance;
}

kan_bool_t kan_rpl_compiler_context_use_module (kan_rpl_compiler_context_t compiler_context,
                                                struct kan_rpl_intermediate_t *intermediate_reference)
{
    struct rpl_compiler_context_t *instance = (struct rpl_compiler_context_t *) compiler_context;
    for (uint64_t module_index = 0u; module_index < instance->modules.size; ++module_index)
    {
        if (((struct kan_rpl_intermediate_t **) instance->modules.data)[module_index] == intermediate_reference)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Caught attempt to use module \"%s\" twice.",
                     instance->log_name, intermediate_reference->log_name)
            return KAN_TRUE;
        }
    }

    for (uint64_t new_option_index = 0u; new_option_index < intermediate_reference->options.size; ++new_option_index)
    {
        struct kan_rpl_option_t *new_option =
            &((struct kan_rpl_option_t *) intermediate_reference->options.data)[new_option_index];

        for (uint64_t old_option_index = 0u; old_option_index < instance->option_values.size; ++old_option_index)
        {
            struct rpl_compiler_context_option_value_t *old_option =
                &((struct rpl_compiler_context_option_value_t *) instance->option_values.data)[old_option_index];

            if (old_option->name == new_option->name)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING,
                         "[%s] Unable to use module \"%s\" as it contains option \"%s\" which is already declared in "
                         "other used module.",
                         instance->log_name, intermediate_reference->log_name, new_option->name)
                return KAN_FALSE;
            }
        }
    }

    struct kan_rpl_intermediate_t **spot = kan_dynamic_array_add_last (&instance->modules);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (&instance->modules, KAN_MAX (1u, instance->modules.size * 2u));
        spot = kan_dynamic_array_add_last (&instance->modules);
        KAN_ASSERT (spot)
    }

    *spot = intermediate_reference;
    kan_dynamic_array_set_capacity (&instance->option_values,
                                    instance->option_values.size + intermediate_reference->options.size);

    for (uint64_t new_option_index = 0u; new_option_index < intermediate_reference->options.size; ++new_option_index)
    {
        struct kan_rpl_option_t *new_option =
            &((struct kan_rpl_option_t *) intermediate_reference->options.data)[new_option_index];

        struct rpl_compiler_context_option_value_t *option_value =
            kan_dynamic_array_add_last (&instance->option_values);
        KAN_ASSERT (option_value)

        option_value->name = new_option->name;
        option_value->scope = new_option->scope;
        option_value->type = new_option->type;

        switch (new_option->type)
        {
        case KAN_RPL_OPTION_TYPE_FLAG:
            option_value->flag_value = new_option->flag_default_value;
            break;

        case KAN_RPL_OPTION_TYPE_COUNT:
            option_value->count_value = new_option->count_default_value;
            break;
        }
    }

    return KAN_TRUE;
}

kan_bool_t kan_rpl_compiler_context_set_option_flag (kan_rpl_compiler_context_t compiler_context,
                                                     kan_interned_string_t name,
                                                     kan_bool_t value)
{
    struct rpl_compiler_context_t *instance = (struct rpl_compiler_context_t *) compiler_context;
    for (uint64_t index = 0u; index < instance->option_values.size; ++index)
    {
        struct rpl_compiler_context_option_value_t *option =
            &((struct rpl_compiler_context_option_value_t *) instance->option_values.data)[index];

        if (option->name == name)
        {
            if (option->type != KAN_RPL_OPTION_TYPE_FLAG)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Option \"%s\" is not a flag.", instance->log_name,
                         name)
                return KAN_FALSE;
            }

            option->flag_value = value;
            return KAN_TRUE;
        }
    }

    KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Unable to find flag option \"%s\".", instance->log_name, name)
    return KAN_FALSE;
}

kan_bool_t kan_rpl_compiler_context_set_option_count (kan_rpl_compiler_context_t compiler_context,
                                                      kan_interned_string_t name,
                                                      uint64_t value)
{
    struct rpl_compiler_context_t *instance = (struct rpl_compiler_context_t *) compiler_context;
    for (uint64_t index = 0u; index < instance->option_values.size; ++index)
    {
        struct rpl_compiler_context_option_value_t *option =
            &((struct rpl_compiler_context_option_value_t *) instance->option_values.data)[index];

        if (option->name == name)
        {
            if (option->type != KAN_RPL_OPTION_TYPE_COUNT)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Option \"%s\" is not a count.",
                         instance->log_name, name)
                return KAN_FALSE;
            }

            option->count_value = value;
            return KAN_TRUE;
        }
    }

    KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Unable to find count option \"%s\".", instance->log_name,
             name)
    return KAN_FALSE;
}

static struct compile_time_evaluation_value_t evaluate_compile_time_expression (
    struct rpl_compiler_context_t *instance,
    kan_interned_string_t intermediate_log_name,
    struct kan_rpl_expression_node_t *expression,
    kan_bool_t instance_options_allowed)
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
            struct rpl_compiler_context_option_value_t *option =
                &((struct rpl_compiler_context_option_value_t *) instance->option_values.data)[option_index];

            if (option->name == expression->identifier)
            {
                found = KAN_TRUE;
                if (option->scope == KAN_RPL_OPTION_SCOPE_INSTANCE && !instance_options_allowed)
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Compile time expression contains non-global option \"%s\" in context that "
                             "only allows global options.",
                             instance->log_name, intermediate_log_name, expression->source_name,
                             (long) expression->source_line, expression->identifier)
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
                            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                                     "[%s:%s:%s:%ld] Compile time expression uses count option \"%s\" that has value "
                                     "%llu that is greater that supported in conditionals.",
                                     instance->log_name, intermediate_log_name, expression->source_name,
                                     (long) expression->source_line, expression->identifier,
                                     (unsigned long long) option->count_value)
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
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s%ld] Compile time expression uses option \"%s\" that cannot be found.",
                     instance->log_name, intermediate_log_name, expression->source_name, (long) expression->source_line,
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
            instance, intermediate_log_name, &((struct kan_rpl_expression_node_t *) expression->children.data)[0u],
            instance_options_allowed);
        struct compile_time_evaluation_value_t right_operand = evaluate_compile_time_expression (
            instance, intermediate_log_name, &((struct kan_rpl_expression_node_t *) expression->children.data)[1u],
            instance_options_allowed);

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
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Operator \"%s\" has unsupported operand types.", \
                 instance->log_name, intermediate_log_name, expression->source_name, (long) expression->source_line,   \
                 #OPERATOR)                                                                                            \
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
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Operator \"%s\" has unsupported operand types.", \
                 instance->log_name, intermediate_log_name, expression->source_name, (long) expression->source_line,   \
                 #OPERATOR)                                                                                            \
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
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Operator \"%s\" has unsupported operand types.", \
                 instance->log_name, intermediate_log_name, expression->source_name, (long) expression->source_line,   \
                 #OPERATOR)                                                                                            \
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;                                                         \
    }

        switch (expression->binary_operation)
        {
        case KAN_RPL_BINARY_OPERATION_FIELD_ACCESS:
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Operator \".\" is not supported in compile time expressions.", instance->log_name,
                     intermediate_log_name, expression->source_name, (long) expression->source_line)
            result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            break;

        case KAN_RPL_BINARY_OPERATION_ARRAY_ACCESS:
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Operator \"[]\" is not supported in compile time expressions.", instance->log_name,
                     intermediate_log_name, expression->source_name, (long) expression->source_line)
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
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Operator \"%%\" has unsupported operand types.", instance->log_name,
                         instance->log_name, expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
            }

            break;

        case KAN_RPL_BINARY_OPERATION_ASSIGN:
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Operator \"=\" is not supported in conditionals.", instance->log_name,
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
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Operator \"&&\" has unsupported operand types.", instance->log_name,
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
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Operator \"||\" has unsupported operand types.", instance->log_name,
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
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Operator \"==\" has unsupported operand types.", instance->log_name,
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
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Operator \"!=\" has unsupported operand types.", instance->log_name,
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
            instance, intermediate_log_name, &((struct kan_rpl_expression_node_t *) expression->children.data)[0u],
            instance_options_allowed);
        result.type = operand.type;

        switch (expression->unary_operation)
        {
        case KAN_RPL_UNARY_OPERATION_NEGATE:
            switch (operand.type)
            {
            case CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR:
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN:
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Operator \"-\" cannot be applied to boolean value.", instance->log_name,
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
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Operator \"!\" can only be applied to boolean value.", instance->log_name,
                         instance->log_name, expression->source_name, (long) expression->source_line)
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
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Operator \"~\" can only be applied to integer value.", instance->log_name,
                         instance->log_name, expression->source_name, (long) expression->source_line)
                result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
                break;
            }

            break;
        }

        break;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL:
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Compile time expression contains function call which is not supported.",
                 instance->log_name, instance->log_name, expression->source_name, (long) expression->source_line)
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
        break;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR:
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Compile time expression contains constructor which is not supported.",
                 instance->log_name, instance->log_name, expression->source_name, (long) expression->source_line)
        result.type = CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR;
        break;
    }

    return result;
}

static enum conditional_evaluation_result_t evaluate_conditional (struct rpl_compiler_context_t *instance,
                                                                  kan_interned_string_t intermediate_log_name,
                                                                  struct kan_rpl_expression_node_t *expression,
                                                                  kan_bool_t instance_options_allowed)
{
    if (expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    {
        return CONDITIONAL_EVALUATION_RESULT_TRUE;
    }

    struct compile_time_evaluation_value_t result =
        evaluate_compile_time_expression (instance, intermediate_log_name, expression, instance_options_allowed);

    switch (result.type)
    {
    case CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR:
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Conditional evaluation resulted in failure.",
                 instance->log_name, intermediate_log_name, expression->source_name, (long) expression->source_line)
        return CONDITIONAL_EVALUATION_RESULT_FAILED;

    case CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN:
        return result.boolean_value ? CONDITIONAL_EVALUATION_RESULT_TRUE : CONDITIONAL_EVALUATION_RESULT_FALSE;

    case CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER:
        return result.integer_value != 0 ? CONDITIONAL_EVALUATION_RESULT_TRUE : CONDITIONAL_EVALUATION_RESULT_FALSE;

    case CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING:
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Conditional evaluation resulted in floating value.", instance->log_name,
                 intermediate_log_name, expression->source_name, (long) expression->source_line)
        return CONDITIONAL_EVALUATION_RESULT_FAILED;
    }

    KAN_ASSERT (KAN_FALSE)
    return CONDITIONAL_EVALUATION_RESULT_FAILED;
}

static kan_bool_t resolve_settings (struct rpl_compiler_context_t *context,
                                    struct rpl_compiler_instance_t *instance,
                                    kan_interned_string_t intermediate_log_name,
                                    struct kan_dynamic_array_t *settings_array,
                                    struct compiler_instance_setting_node_t **first_output,
                                    struct compiler_instance_setting_node_t **last_output)
{
    kan_bool_t result = KAN_TRUE;
    for (uint64_t setting_index = 0u; setting_index < settings_array->size; ++setting_index)
    {
        struct kan_rpl_setting_t *source_setting = &((struct kan_rpl_setting_t *) settings_array->data)[setting_index];

        switch (evaluate_conditional (context, intermediate_log_name, &source_setting->conditional, KAN_TRUE))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            result = KAN_FALSE;
            break;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
        {
            struct compiler_instance_setting_node_t *target_setting = kan_stack_group_allocator_allocate (
                &instance->resolve_allocator, sizeof (struct compiler_instance_setting_node_t),
                _Alignof (struct compiler_instance_setting_node_t));

            target_setting->next = NULL;
            target_setting->name = source_setting->name;
            target_setting->type = source_setting->type;
            target_setting->module_name = intermediate_log_name;
            target_setting->source_name = source_setting->source_name;
            target_setting->source_line = source_setting->source_line;

            switch (source_setting->type)
            {
            case KAN_RPL_SETTING_TYPE_FLAG:
                target_setting->flag = source_setting->flag;
                break;

            case KAN_RPL_SETTING_TYPE_INTEGER:
                target_setting->integer = source_setting->integer;
                break;

            case KAN_RPL_SETTING_TYPE_FLOATING:
                target_setting->floating = source_setting->floating;
                break;

            case KAN_RPL_SETTING_TYPE_STRING:
                target_setting->string = source_setting->string;
                break;
            }

            if (*last_output)
            {
                (*last_output)->next = target_setting;
                (*last_output) = target_setting;
            }
            else
            {
                (*first_output) = target_setting;
                (*last_output) = target_setting;
            }

            break;
        }

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            break;
        }
    }

    return result;
}

static inline kan_bool_t resolve_array_dimensions (struct rpl_compiler_context_t *context,
                                                   struct rpl_compiler_instance_t *instance,
                                                   kan_interned_string_t intermediate_log_name,
                                                   struct compiler_instance_variable_t *variable,
                                                   struct kan_dynamic_array_t *dimensions,
                                                   kan_bool_t instance_options_allowed)
{
    kan_bool_t result = KAN_TRUE;
    variable->array_dimensions_count = dimensions->size;

    if (variable->array_dimensions_count > 0u)
    {
        variable->array_dimensions = kan_stack_group_allocator_allocate (
            &instance->resolve_allocator, sizeof (uint64_t) * variable->array_dimensions_count, _Alignof (uint64_t));

        for (uint64_t dimension = 0u; dimension < variable->array_dimensions_count; ++dimension)
        {
            struct kan_rpl_expression_node_t *expression =
                &((struct kan_rpl_expression_node_t *) dimensions->data)[dimension];

            struct compile_time_evaluation_value_t value =
                evaluate_compile_time_expression (context, intermediate_log_name, expression, instance_options_allowed);

            switch (value.type)
            {
            case CONDITIONAL_EVALUATION_VALUE_TYPE_ERROR:
                result = KAN_FALSE;
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_BOOLEAN:
            case CONDITIONAL_EVALUATION_VALUE_TYPE_FLOATING:
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Declaration \"%s\" array size at dimension %ld calculation resulted "
                         "in non-integer value.",
                         context->log_name, intermediate_log_name, expression->source_name,
                         (long) expression->source_line, variable->name, (long) dimension)
                result = KAN_FALSE;
                break;

            case CONDITIONAL_EVALUATION_VALUE_TYPE_INTEGER:
                if (value.integer_value > 0u)
                {
                    variable->array_dimensions[dimension] = (uint64_t) value.integer_value;
                }
                else
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Declaration \"%s\" array size at dimension %ld calculation resulted "
                             "in invalid value for array size %lld.",
                             context->log_name, intermediate_log_name, expression->source_name,
                             (long) expression->source_line, variable->name, (long) dimension,
                             (long long) value.integer_value)
                    result = KAN_FALSE;
                }
                break;
            }
        }
    }
    else
    {
        variable->array_dimensions = NULL;
    }

    return result;
}

static kan_bool_t resolve_use_struct (struct rpl_compiler_context_t *context,
                                      struct rpl_compiler_instance_t *instance,
                                      kan_interned_string_t name,
                                      struct compiler_instance_struct_node_t **output);

static inline kan_bool_t resolve_variable_type (struct rpl_compiler_context_t *context,
                                                struct rpl_compiler_instance_t *instance,
                                                kan_interned_string_t intermediate_log_name,
                                                struct compiler_instance_variable_t *variable,
                                                kan_interned_string_t type_name,
                                                kan_interned_string_t declaration_name,
                                                kan_interned_string_t source_name,
                                                uint64_t source_line)
{
    variable->type_if_vector = NULL;
    variable->type_if_matrix = NULL;
    variable->type_if_struct = NULL;

    if (!(variable->type_if_vector = find_inbuilt_vector_type (type_name)) &&
        !(variable->type_if_matrix = find_inbuilt_matrix_type (type_name)) &&
        !resolve_use_struct (context, instance, type_name, &variable->type_if_struct))
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Declaration \"%s\" type \"%s\" is unknown.",
                 context->log_name, intermediate_log_name, source_name, (long) source_line, declaration_name, type_name)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static kan_bool_t resolve_declarations (struct rpl_compiler_context_t *context,
                                        struct rpl_compiler_instance_t *instance,
                                        kan_interned_string_t intermediate_log_name,
                                        struct kan_dynamic_array_t *declaration_array,
                                        struct compiler_instance_declaration_node_t **first_output,
                                        kan_bool_t instance_options_allowed)
{
    kan_bool_t result = KAN_TRUE;
    uint64_t current_offset = 0u;

    struct compiler_instance_declaration_node_t *first = NULL;
    struct compiler_instance_declaration_node_t *last = NULL;

    for (uint64_t declaration_index = 0u; declaration_index < declaration_array->size; ++declaration_index)
    {
        struct kan_rpl_declaration_t *source_declaration =
            &((struct kan_rpl_declaration_t *) declaration_array->data)[declaration_index];

        switch (evaluate_conditional (context, intermediate_log_name, &source_declaration->conditional,
                                      instance_options_allowed))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            result = KAN_FALSE;
            break;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
        {
            struct compiler_instance_declaration_node_t *target_declaration = kan_stack_group_allocator_allocate (
                &instance->resolve_allocator, sizeof (struct compiler_instance_declaration_node_t),
                _Alignof (struct compiler_instance_declaration_node_t));

            target_declaration->next = NULL;
            target_declaration->variable.name = source_declaration->name;
            target_declaration->variable.type_if_vector = NULL;
            target_declaration->variable.type_if_matrix = NULL;
            target_declaration->variable.type_if_struct = NULL;

            if (!resolve_variable_type (context, instance, intermediate_log_name, &target_declaration->variable,
                                        source_declaration->type_name, source_declaration->name,
                                        source_declaration->source_name, source_declaration->source_line))
            {
                result = KAN_FALSE;
            }

            if (!resolve_array_dimensions (context, instance, intermediate_log_name, &target_declaration->variable,
                                           &source_declaration->array_sizes, instance_options_allowed))
            {
                result = KAN_FALSE;
            }

            if (result)
            {
                target_declaration->size = 0u;
                target_declaration->alignment = 0u;

                if (target_declaration->variable.type_if_vector)
                {
                    target_declaration->size =
                        inbuilt_type_item_size[target_declaration->variable.type_if_vector->item] *
                        target_declaration->variable.type_if_vector->items_count;
                    target_declaration->alignment =
                        inbuilt_type_item_size[target_declaration->variable.type_if_vector->item];
                }
                else if (target_declaration->variable.type_if_matrix)
                {
                    target_declaration->size =
                        inbuilt_type_item_size[target_declaration->variable.type_if_matrix->item] *
                        target_declaration->variable.type_if_matrix->rows *
                        target_declaration->variable.type_if_matrix->columns;
                    target_declaration->alignment =
                        inbuilt_type_item_size[target_declaration->variable.type_if_matrix->item];
                }
                else if (target_declaration->variable.type_if_struct)
                {
                    target_declaration->size = target_declaration->variable.type_if_struct->size;
                    target_declaration->alignment = target_declaration->variable.type_if_struct->alignment;
                }

                if (target_declaration->size != 0u && target_declaration->alignment != 0u)
                {
                    for (uint64_t dimension = 0u; dimension < target_declaration->variable.array_dimensions_count; ++dimension)
                    {
                        target_declaration->size *= target_declaration->variable.array_dimensions[dimension];
                    }

                    current_offset = kan_apply_alignment (current_offset, target_declaration->alignment);
                    target_declaration->offset = current_offset;
                    current_offset += target_declaration->size;
                }
            }

            target_declaration->meta_count = source_declaration->meta.size;
            if (target_declaration->meta_count > 0u)
            {
                target_declaration->meta = kan_stack_group_allocator_allocate (
                    &instance->resolve_allocator, sizeof (kan_interned_string_t) * target_declaration->meta_count,
                    _Alignof (kan_interned_string_t));
                memcpy (target_declaration->meta, source_declaration->meta.data,
                        sizeof (kan_interned_string_t) * target_declaration->meta_count);
            }
            else
            {
                target_declaration->meta = NULL;
            }

            target_declaration->module_name = intermediate_log_name;
            target_declaration->source_name = source_declaration->source_name;
            target_declaration->source_line = source_declaration->source_line;

            if (last)
            {
                last->next = target_declaration;
                last = target_declaration;
            }
            else
            {
                first = target_declaration;
                last = target_declaration;
            }

            break;
        }

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            break;
        }
    }

    if (result)
    {
        *first_output = first;
    }

    return result;
}

static inline void flattening_name_generation_buffer_reset (struct flattening_name_generation_buffer_t *buffer,
                                                            uint64_t to_length)
{
    buffer->length = KAN_MIN (KAN_RPL_COMPILER_INSTANCE_MAX_FLAT_NAME_LENGTH - 1u, to_length);
    buffer->buffer[buffer->length] = '\0';
}

static inline void flattening_name_generation_buffer_append (struct flattening_name_generation_buffer_t *buffer,
                                                             const char *name)
{
    const uint64_t sub_name_length = strlen (name);
    const uint64_t new_length = buffer->length + 1u + sub_name_length;
    const uint64_t dot_position = buffer->length;
    const uint64_t sub_name_position = dot_position + 1u;
    flattening_name_generation_buffer_reset (buffer, new_length);

    if (dot_position < buffer->length)
    {
        buffer->buffer[dot_position] = '.';
    }

    if (sub_name_position < buffer->length)
    {
        const uint64_t to_copy = buffer->length - sub_name_position;
        memcpy (&buffer->buffer[sub_name_position], name, to_copy);
    }
}

static kan_bool_t flatten_buffer_process_field (struct rpl_compiler_context_t *context,
                                                struct rpl_compiler_instance_t *instance,
                                                struct compiler_instance_buffer_node_t *buffer,
                                                struct compiler_instance_declaration_node_t *declaration,
                                                struct compiler_instance_buffer_flattening_graph_node_t *output_node,
                                                struct flattening_name_generation_buffer_t *name_generation_buffer,
                                                struct binding_location_assignment_counter_t *assignment_counter);

static kan_bool_t flatten_buffer_process_field_list (
    struct rpl_compiler_context_t *context,
    struct rpl_compiler_instance_t *instance,
    struct compiler_instance_buffer_node_t *buffer,
    struct compiler_instance_declaration_node_t *first_declaration,
    struct compiler_instance_buffer_flattening_graph_node_t *output_node,
    struct flattening_name_generation_buffer_t *name_generation_buffer,
    struct binding_location_assignment_counter_t *assignment_counter)
{
    kan_bool_t result = KAN_TRUE;
    struct compiler_instance_buffer_flattening_graph_node_t *last_root = NULL;
    struct compiler_instance_declaration_node_t *field = first_declaration;

    while (field)
    {
        struct compiler_instance_buffer_flattening_graph_node_t *new_root = kan_stack_group_allocator_allocate (
            &instance->resolve_allocator, sizeof (struct compiler_instance_buffer_flattening_graph_node_t),
            _Alignof (struct compiler_instance_buffer_flattening_graph_node_t));

        new_root->next_on_level = NULL;
        new_root->first_child = NULL;
        new_root->name = field->variable.name;
        new_root->flattened_result = NULL;

        const uint64_t length = name_generation_buffer->length;
        flattening_name_generation_buffer_append (name_generation_buffer, field->variable.name);

        if (!flatten_buffer_process_field (context, instance, buffer, field, new_root, name_generation_buffer,
                                           assignment_counter))
        {
            result = KAN_FALSE;
        }

        flattening_name_generation_buffer_reset (name_generation_buffer, length);
        if (last_root)
        {
            last_root->next_on_level = new_root;
        }
        else
        {
            if (output_node)
            {
                output_node->first_child = new_root;
            }
            else
            {
                buffer->flattening_graph_base = new_root;
            }
        }

        last_root = new_root;
        field = field->next;
    }

    return result;
}

static kan_bool_t flatten_buffer_process_field (struct rpl_compiler_context_t *context,
                                                struct rpl_compiler_instance_t *instance,
                                                struct compiler_instance_buffer_node_t *buffer,
                                                struct compiler_instance_declaration_node_t *declaration,
                                                struct compiler_instance_buffer_flattening_graph_node_t *output_node,
                                                struct flattening_name_generation_buffer_t *name_generation_buffer,
                                                struct binding_location_assignment_counter_t *assignment_counter)
{
    kan_bool_t result = KAN_TRUE;
    if (declaration->variable.type_if_vector || declaration->variable.type_if_matrix)
    {
        // Reached leaf.
        struct compiler_instance_buffer_flattened_declaration_t *flattened = kan_stack_group_allocator_allocate (
            &instance->resolve_allocator, sizeof (struct compiler_instance_buffer_flattened_declaration_t),
            _Alignof (struct compiler_instance_buffer_flattened_declaration_t));

        flattened->next = NULL;
        flattened->source_declaration = declaration;
        flattened->readable_name = kan_string_intern (name_generation_buffer->buffer);

        switch (buffer->type)
        {
        case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
            flattened->location = assignment_counter->next_attribute_location;
            ++assignment_counter->next_attribute_location;
            break;

        case KAN_RPL_BUFFER_TYPE_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
            KAN_ASSERT (KAN_FALSE)
            break;

        case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
            flattened->location = assignment_counter->next_vertex_output_location;
            ++assignment_counter->next_vertex_output_location;
            break;

        case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
            flattened->location = assignment_counter->next_fragment_output_location;
            ++assignment_counter->next_fragment_output_location;
            break;
        }

        if (buffer->last_flattened_declaration)
        {
            buffer->last_flattened_declaration->next = flattened;
        }
        else
        {
            buffer->first_flattened_declaration = flattened;
        }

        buffer->last_flattened_declaration = flattened;
        output_node->flattened_result = flattened;
    }
    else if (declaration->variable.type_if_struct)
    {
        if (!flatten_buffer_process_field_list (context, instance, buffer,
                                                declaration->variable.type_if_struct->first_field, output_node,
                                                name_generation_buffer, assignment_counter))
        {
            result = KAN_FALSE;
        }
    }

    return result;
}

static kan_bool_t flatten_buffer (struct rpl_compiler_context_t *context,
                                  struct rpl_compiler_instance_t *instance,
                                  struct compiler_instance_buffer_node_t *buffer,
                                  struct binding_location_assignment_counter_t *assignment_counter)
{
    kan_bool_t result = KAN_TRUE;
    struct flattening_name_generation_buffer_t name_generation_buffer;
    const uint64_t buffer_name_length = strlen (buffer->name);
    const uint64_t to_copy = KAN_MIN (KAN_RPL_COMPILER_INSTANCE_MAX_FLAT_NAME_LENGTH - 1u, buffer_name_length);

    flattening_name_generation_buffer_reset (&name_generation_buffer, to_copy);
    memcpy (name_generation_buffer.buffer, buffer->name, to_copy);

    if (!flatten_buffer_process_field_list (context, instance, buffer, buffer->first_field, NULL,
                                            &name_generation_buffer, assignment_counter))
    {
        result = KAN_FALSE;
    }

    return result;
}

static kan_bool_t resolve_buffers_validate_uniform_internals_alignment (
    struct rpl_compiler_context_t *context,
    struct compiler_instance_buffer_node_t *buffer,
    struct compiler_instance_declaration_node_t *first_declaration)
{
    kan_bool_t valid = KAN_TRUE;
    struct compiler_instance_declaration_node_t *declaration = first_declaration;

    while (declaration)
    {
        if (declaration->variable.type_if_vector)
        {
            const uint32_t size = declaration->variable.type_if_vector->items_count *
                                  inbuilt_type_item_size[declaration->variable.type_if_vector->item];

            if (size % 16u != 0u)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Declaration \"%s\" is found inside buffer \"%s\", but its size is not "
                         "multiple of 16, which is prone to cause errors when used with uniform buffers.",
                         context->log_name, declaration->module_name, declaration->source_name,
                         (long) declaration->source_line, declaration->variable.name, buffer->name)
                valid = KAN_FALSE;
            }
        }
        else if (declaration->variable.type_if_matrix)
        {
            const uint32_t size = declaration->variable.type_if_matrix->rows *
                                  declaration->variable.type_if_matrix->columns *
                                  inbuilt_type_item_size[declaration->variable.type_if_matrix->item];

            if (size % 16u != 0u)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Declaration \"%s\" is found inside buffer \"%s\", but its size is not "
                         "multiple of 16, which is prone to cause errors when used with uniform buffers.",
                         context->log_name, declaration->module_name, declaration->source_name,
                         (long) declaration->source_line, declaration->variable.name, buffer->name)
                valid = KAN_FALSE;
            }
        }
        else if (declaration->variable.type_if_struct)
        {
            if (!resolve_buffers_validate_uniform_internals_alignment (
                    context, buffer, declaration->variable.type_if_struct->first_field))
            {
                valid = KAN_FALSE;
            }
        }

        declaration = declaration->next;
    }

    return valid;
}

static kan_bool_t is_global_name_occupied (struct rpl_compiler_instance_t *instance, kan_interned_string_t name)
{
    struct compiler_instance_struct_node_t *struct_data = instance->first_struct;
    while (struct_data)
    {
        if (struct_data->name == name)
        {
            return KAN_TRUE;
        }

        struct_data = struct_data->next;
    }

    struct compiler_instance_buffer_node_t *buffer = instance->first_buffer;
    while (buffer)
    {
        if (buffer->name == name)
        {
            return KAN_TRUE;
        }

        buffer = buffer->next;
    }

    struct compiler_instance_sampler_node_t *sampler = instance->first_sampler;
    while (sampler)
    {
        if (sampler->name == name)
        {
            return KAN_TRUE;
        }

        sampler = sampler->next;
    }

    struct compiler_instance_function_node_t *function = instance->first_function;
    while (function)
    {
        if (function->name == name)
        {
            return KAN_TRUE;
        }

        function = function->next;
    }

    return KAN_FALSE;
}

static inline void calculate_size_and_alignment_from_declarations (
    struct compiler_instance_declaration_node_t *declaration, uint64_t *size_output, uint64_t *alignment_output)
{
    *size_output = 0u;
    *alignment_output = 1u;

    while (declaration)
    {
        *size_output = declaration->offset + declaration->size;
        *alignment_output = KAN_MAX (*alignment_output, declaration->alignment);
        declaration = declaration->next;
    }

    *size_output = kan_apply_alignment (*size_output, *alignment_output);
}

static kan_bool_t resolve_buffers (struct rpl_compiler_context_t *context,
                                   struct rpl_compiler_instance_t *instance,
                                   struct kan_rpl_intermediate_t *intermediate,
                                   struct binding_location_assignment_counter_t *assignment_counter)
{
    kan_bool_t result = KAN_TRUE;
    for (uint64_t buffer_index = 0u; buffer_index < intermediate->buffers.size; ++buffer_index)
    {
        struct kan_rpl_buffer_t *source_buffer =
            &((struct kan_rpl_buffer_t *) intermediate->buffers.data)[buffer_index];

        switch (evaluate_conditional (context, intermediate->log_name, &source_buffer->conditional, KAN_FALSE))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            result = KAN_FALSE;
            break;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
        {
            if (is_global_name_occupied (instance, source_buffer->name))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Cannot resolve buffer \"%s\" as its global name is already occupied.",
                         context->log_name, intermediate->log_name, source_buffer->source_name,
                         (long) source_buffer->source_line, source_buffer->name)

                result = KAN_FALSE;
                break;
            }

            struct compiler_instance_buffer_node_t *target_buffer = kan_stack_group_allocator_allocate (
                &instance->resolve_allocator, sizeof (struct compiler_instance_buffer_node_t),
                _Alignof (struct compiler_instance_buffer_node_t));

            target_buffer->next = NULL;
            target_buffer->name = source_buffer->name;
            target_buffer->type = source_buffer->type;
            target_buffer->used = KAN_FALSE;

            if (!resolve_declarations (context, instance, intermediate->log_name, &source_buffer->fields,
                                       &target_buffer->first_field, KAN_FALSE))
            {
                result = KAN_FALSE;
            }

            switch (target_buffer->type)
            {
            case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
                target_buffer->binding = assignment_counter->next_attribute_buffer_binding;
                ++assignment_counter->next_attribute_buffer_binding;
                break;

            case KAN_RPL_BUFFER_TYPE_UNIFORM:
            case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
                target_buffer->binding = assignment_counter->next_arbitrary_buffer_binding;
                ++assignment_counter->next_arbitrary_buffer_binding;
                break;

            case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
            case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
                // Not an external buffers, so no binding.
                target_buffer->binding = INVALID_BINDING;
                break;
            }

            target_buffer->flattening_graph_base = NULL;
            target_buffer->first_flattened_declaration = NULL;
            target_buffer->last_flattened_declaration = NULL;

            if (result)
            {
                switch (target_buffer->type)
                {
                case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
                case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
                {
                    flatten_buffer (context, instance, target_buffer, assignment_counter);
                    struct compiler_instance_buffer_flattened_declaration_t *declaration =
                        target_buffer->first_flattened_declaration;

                    while (declaration)
                    {
                        if (declaration->source_declaration->variable.array_dimensions_count > 0u)
                        {
                            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                                     "[%s:%s:%s:%ld] Attributes should not be arrays, but flattened declaration \"%s\" "
                                     "with array suffix found.",
                                     context->log_name, intermediate->log_name,
                                     declaration->source_declaration->source_name,
                                     (long) declaration->source_declaration->source_line, declaration->readable_name)
                            result = KAN_FALSE;
                        }

                        declaration = declaration->next;
                    }

                    break;
                }

                case KAN_RPL_BUFFER_TYPE_UNIFORM:
                case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
                    if (!resolve_buffers_validate_uniform_internals_alignment (context, target_buffer,
                                                                               target_buffer->first_field))
                    {
                        result = KAN_FALSE;
                    }

                    break;

                case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
                case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
                    break;

                case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
                    flatten_buffer (context, instance, target_buffer, assignment_counter);
                    break;

                case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
                {
                    flatten_buffer (context, instance, target_buffer, assignment_counter);
                    struct compiler_instance_buffer_flattened_declaration_t *declaration =
                        target_buffer->first_flattened_declaration;

                    while (declaration)
                    {
                        if (declaration->source_declaration->variable.type_if_vector != &type_f4)
                        {
                            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                                     "[%s:%s:%s:%ld] Fragment stage output should only contain \"f4\" declarations, "
                                     "but flattened declaration \"%s\" with other type found.",
                                     context->log_name, intermediate->log_name,
                                     declaration->source_declaration->source_name,
                                     (long) declaration->source_declaration->source_line, declaration->readable_name)
                            result = KAN_FALSE;
                        }

                        declaration = declaration->next;
                    }

                    break;
                }
                }
            }

            calculate_size_and_alignment_from_declarations (target_buffer->first_field, &target_buffer->size,
                                                            &target_buffer->alignment);

            target_buffer->module_name = intermediate->log_name;
            target_buffer->source_name = source_buffer->source_name;
            target_buffer->source_line = source_buffer->source_line;

            if (instance->last_buffer)
            {
                instance->last_buffer->next = target_buffer;
                instance->last_buffer = target_buffer;
            }
            else
            {
                instance->first_buffer = target_buffer;
                instance->last_buffer = target_buffer;
            }

            break;
        }

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            break;
        }
    }

    return result;
}

static kan_bool_t resolve_samplers (struct rpl_compiler_context_t *context,
                                    struct rpl_compiler_instance_t *instance,
                                    struct kan_rpl_intermediate_t *intermediate,
                                    struct binding_location_assignment_counter_t *assignment_counter)
{
    kan_bool_t result = KAN_TRUE;
    for (uint64_t sampler_index = 0u; sampler_index < intermediate->samplers.size; ++sampler_index)
    {
        struct kan_rpl_sampler_t *source_sampler =
            &((struct kan_rpl_sampler_t *) intermediate->samplers.data)[sampler_index];

        switch (evaluate_conditional (context, intermediate->log_name, &source_sampler->conditional, KAN_FALSE))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            result = KAN_FALSE;
            break;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
        {
            if (is_global_name_occupied (instance, source_sampler->name))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Cannot resolve sampler \"%s\" as its global name is already occupied.",
                         context->log_name, intermediate->log_name, source_sampler->source_name,
                         (long) source_sampler->source_line, source_sampler->name)

                result = KAN_FALSE;
                break;
            }

            struct compiler_instance_sampler_node_t *target_sampler = kan_stack_group_allocator_allocate (
                &instance->resolve_allocator, sizeof (struct compiler_instance_sampler_node_t),
                _Alignof (struct compiler_instance_sampler_node_t));

            target_sampler->next = NULL;
            target_sampler->name = source_sampler->name;
            target_sampler->type = source_sampler->type;

            target_sampler->used = KAN_FALSE;
            target_sampler->binding = assignment_counter->next_arbitrary_buffer_binding;
            ++assignment_counter->next_arbitrary_buffer_binding;

            struct compiler_instance_setting_node_t *first_setting = NULL;
            struct compiler_instance_setting_node_t *last_setting = NULL;

            if (!resolve_settings (context, instance, intermediate->log_name, &source_sampler->settings, &first_setting,
                                   &last_setting))
            {
                result = KAN_FALSE;
            }

            target_sampler->first_setting = first_setting;
            target_sampler->module_name = intermediate->log_name;
            target_sampler->source_name = source_sampler->source_name;
            target_sampler->source_line = source_sampler->source_line;

            if (instance->last_sampler)
            {
                instance->last_sampler->next = target_sampler;
                instance->last_sampler = target_sampler;
            }
            else
            {
                instance->first_sampler = target_sampler;
                instance->last_sampler = target_sampler;
            }

            break;
        }

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            break;
        }
    }

    return result;
}

static const char *get_stage_name (enum kan_rpl_pipeline_stage_t stage)
{
    switch (stage)
    {
    case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX:
        return "classic_pipeline_vertex_stage";
    case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT:
        return "classic_pipeline_fragment_stage";
    }

    return "unknown_pipeline_stage";
}

static kan_bool_t check_alias_or_variable_name_is_not_occupied (struct rpl_compiler_instance_t *instance,
                                                                struct resolve_expression_scope_t *resolve_scope,
                                                                kan_interned_string_t name)
{
    struct resolve_expression_alias_node_t *alias_node = resolve_scope->first_alias;
    while (alias_node)
    {
        if (alias_node->name == name)
        {
            return KAN_FALSE;
        }

        alias_node = alias_node->next;
    }

    if (resolve_scope->associated_resolved_scope_if_any)
    {
        struct compiler_instance_scope_variable_item_t *variable =
            resolve_scope->associated_resolved_scope_if_any->scope.first_variable;

        while (variable)
        {
            if (variable->variable->name == name)
            {
                return KAN_FALSE;
            }

            variable = variable->next;
        }
    }

    if (resolve_scope->parent)
    {
        return check_alias_or_variable_name_is_not_occupied (instance, resolve_scope->parent, name);
    }

    return !is_global_name_occupied (instance, name);
}

static struct resolve_expression_alias_node_t *resolve_find_alias (struct resolve_expression_scope_t *resolve_scope,
                                                                   kan_interned_string_t identifier)
{
    struct resolve_expression_alias_node_t *alias_node = resolve_scope->first_alias;
    while (alias_node)
    {
        if (alias_node->name == identifier)
        {
            return alias_node;
        }

        alias_node = alias_node->next;
    }

    if (resolve_scope->parent)
    {
        return resolve_find_alias (resolve_scope->parent, identifier);
    }

    return NULL;
}

static kan_bool_t is_buffer_can_be_accessed_from_stage (struct compiler_instance_buffer_node_t *buffer,
                                                        enum kan_rpl_pipeline_stage_t stage,
                                                        kan_bool_t *output_unique_stage_binding)
{
    *output_unique_stage_binding = KAN_FALSE;
    switch (buffer->type)
    {
    case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
    case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
    case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
    case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
        *output_unique_stage_binding = KAN_TRUE;
        return stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX;

    case KAN_RPL_BUFFER_TYPE_UNIFORM:
    case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
        return KAN_TRUE;

    case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
        *output_unique_stage_binding = KAN_TRUE;
        return stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX ||
               stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT;

    case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
        *output_unique_stage_binding = KAN_TRUE;
        return stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_TRUE;
}

static kan_bool_t resolve_use_struct (struct rpl_compiler_context_t *context,
                                      struct rpl_compiler_instance_t *instance,
                                      kan_interned_string_t name,
                                      struct compiler_instance_struct_node_t **output)
{
    *output = NULL;
    struct compiler_instance_struct_node_t *struct_node = instance->first_struct;

    while (struct_node)
    {
        if (struct_node->name == name)
        {
            *output = struct_node;
            // Already resolved.
            return KAN_TRUE;
        }

        struct_node = struct_node->next;
    }

    kan_bool_t resolve_successful = KAN_TRUE;
    struct kan_rpl_struct_t *intermediate_struct = NULL;
    kan_interned_string_t intermediate_log_name = NULL;

    for (uint64_t intermediate_index = 0u; intermediate_index < context->modules.size; ++intermediate_index)
    {
        struct kan_rpl_intermediate_t *intermediate =
            ((struct kan_rpl_intermediate_t **) context->modules.data)[intermediate_index];

        for (uint64_t struct_index = 0u; struct_index < intermediate->structs.size; ++struct_index)
        {
            struct kan_rpl_struct_t *struct_data =
                &((struct kan_rpl_struct_t *) intermediate->structs.data)[struct_index];

            if (struct_data->name == name)
            {
                switch (evaluate_conditional (context, intermediate->log_name, &struct_data->conditional, KAN_FALSE))
                {
                case CONDITIONAL_EVALUATION_RESULT_FAILED:
                    resolve_successful = KAN_FALSE;
                    break;

                case CONDITIONAL_EVALUATION_RESULT_TRUE:
                    if (intermediate_struct)
                    {
                        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                                 "[%s:%s:%s:%ld] Encountered duplicate active definition of struct \"%s\".",
                                 context->log_name, intermediate->log_name, struct_data->source_name,
                                 (long) struct_data->source_line, struct_data->name)
                        resolve_successful = KAN_FALSE;
                    }

                    intermediate_struct = struct_data;
                    intermediate_log_name = intermediate->log_name;
                    break;

                case CONDITIONAL_EVALUATION_RESULT_FALSE:
                    break;
                }
            }
        }
    }

    if (!resolve_successful)
    {
        return KAN_FALSE;
    }

    if (!intermediate_struct)
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s] Unable to find struct \"%s\".", context->log_name, name)
        return KAN_FALSE;
    }

    struct_node = kan_stack_group_allocator_allocate (&instance->resolve_allocator,
                                                      sizeof (struct compiler_instance_struct_node_t),
                                                      _Alignof (struct compiler_instance_struct_node_t));
    *output = struct_node;

    struct_node->name = name;
    struct_node->module_name = intermediate_log_name;
    struct_node->source_name = intermediate_struct->source_name;
    struct_node->source_line = intermediate_struct->source_line;

    if (!resolve_declarations (context, instance, intermediate_log_name, &intermediate_struct->fields,
                               &struct_node->first_field, KAN_FALSE))
    {
        resolve_successful = KAN_FALSE;
    }

    calculate_size_and_alignment_from_declarations (struct_node->first_field, &struct_node->size,
                                                    &struct_node->alignment);
    struct_node->next = NULL;

    if (instance->last_struct)
    {
        instance->last_struct->next = struct_node;
    }
    else
    {
        instance->first_struct = struct_node;
    }

    instance->last_struct = struct_node;
    return resolve_successful;
}

static kan_bool_t resolve_use_buffer (struct rpl_compiler_context_t *context,
                                      struct rpl_compiler_instance_t *instance,
                                      struct compiler_instance_function_node_t *function,
                                      uint64_t usage_line,
                                      enum kan_rpl_pipeline_stage_t stage,
                                      struct compiler_instance_buffer_node_t *buffer)
{
    struct compiler_instance_buffer_access_node_t *access_node = function->first_buffer_access;
    while (access_node)
    {
        if (access_node->buffer == buffer)
        {
            // Already used, no need for further verification.
            return KAN_TRUE;
        }

        access_node = access_node->next;
    }

    access_node = kan_stack_group_allocator_allocate (&instance->resolve_allocator,
                                                      sizeof (struct compiler_instance_buffer_access_node_t),
                                                      _Alignof (struct compiler_instance_buffer_access_node_t));

    access_node->next = function->first_buffer_access;
    function->first_buffer_access = access_node;
    access_node->buffer = buffer;
    access_node->direct_access_function = function;

    kan_bool_t needs_binding;
    if (!is_buffer_can_be_accessed_from_stage (buffer, stage, &needs_binding))
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Function \"%s\" is called in stage \"%s\" and tries to access buffer \"%s\" which is "
                 "not accessible in that stage.",
                 context->log_name, function->module_name, function->source_name, (long) usage_line, function->name,
                 get_stage_name (stage), buffer->name)
        return KAN_FALSE;
    }

    if (needs_binding)
    {
        if (function->has_stage_specific_access)
        {
            if (function->required_stage != stage)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Function \"%s\" is already bound to stage \"%s\" and cannot be accessed from "
                         "other stages due to its buffer accesses, but it also tries to access buffer \"%s\" that "
                         "wants to bind function to stage \"%s\".",
                         context->log_name, function->module_name, function->source_name, (long) usage_line,
                         function->name, get_stage_name (function->required_stage), buffer->name,
                         get_stage_name (stage))
                return KAN_FALSE;
            }
        }
        else
        {
            function->has_stage_specific_access = KAN_TRUE;
            function->required_stage = stage;
        }
    }

    buffer->used = KAN_TRUE;
    return KAN_TRUE;
}

static kan_bool_t resolve_use_sampler (struct rpl_compiler_instance_t *instance,
                                       struct compiler_instance_function_node_t *function,
                                       struct compiler_instance_sampler_node_t *sampler)
{
    struct compiler_instance_sampler_access_node_t *access_node = function->first_sampler_access;
    while (access_node)
    {
        if (access_node->sampler == sampler)
        {
            // Already used, no need fop further verification.
            return KAN_TRUE;
        }

        access_node = access_node->next;
    }

    access_node = kan_stack_group_allocator_allocate (&instance->resolve_allocator,
                                                      sizeof (struct compiler_instance_sampler_access_node_t),
                                                      _Alignof (struct compiler_instance_sampler_access_node_t));

    access_node->next = function->first_sampler_access;
    function->first_sampler_access = access_node;
    access_node->sampler = sampler;
    access_node->direct_access_function = function;

    sampler->used = KAN_TRUE;
    return KAN_TRUE;
}

static struct compiler_instance_scope_variable_item_t *resolve_find_variable (
    struct resolve_expression_scope_t *resolve_scope, kan_interned_string_t variable_name)
{
    if (resolve_scope->associated_resolved_scope_if_any)
    {
        struct compiler_instance_scope_variable_item_t *variable =
            resolve_scope->associated_resolved_scope_if_any->scope.first_variable;

        while (variable)
        {
            if (variable->variable->name == variable_name)
            {
                return variable;
            }

            variable = variable->next;
        }
    }

    if (resolve_scope->parent)
    {
        return resolve_find_variable (resolve_scope->parent, variable_name);
    }

    struct compiler_instance_scope_variable_item_t *variable = resolve_scope->function->first_argument_variable;
    while (variable)
    {
        if (variable->variable->name == variable_name)
        {
            return variable;
        }

        variable = variable->next;
    }

    return NULL;
}

static kan_bool_t resolve_expression (struct rpl_compiler_context_t *context,
                                      struct rpl_compiler_instance_t *instance,
                                      struct resolve_expression_scope_t *resolve_scope,
                                      struct kan_rpl_expression_node_t *expression,
                                      struct compiler_instance_expression_node_t **output,
                                      struct resolve_expression_output_type_t *output_type);

static inline const char *get_type_name_for_logging (struct inbuilt_vector_type_t *if_vector,
                                                     struct inbuilt_matrix_type_t *if_matrix,
                                                     struct compiler_instance_struct_node_t *if_struct)
{
    if (if_vector)
    {
        return if_vector->name;
    }
    else if (if_matrix)
    {
        return if_matrix->name;
    }
    else if (if_struct)
    {
        return if_struct->name;
    }

    return "<not_a_variable_type>";
}

static const char *get_expression_call_name_for_logging (struct compiler_instance_expression_node_t *owner_expression)
{
    if (owner_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_FUNCTION_CALL)
    {
        return owner_expression->function_call.function->name;
    }
    else if (owner_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_SAMPLER_CALL)
    {
        return owner_expression->sampler_call.sampler->name;
    }
    else if (owner_expression->type == COMPILER_INSTANCE_EXPRESSION_TYPE_CONSTRUCTOR)
    {
        return get_type_name_for_logging (owner_expression->constructor.type_if_vector,
                                          owner_expression->constructor.type_if_matrix,
                                          owner_expression->constructor.type_if_struct);
    }

    return "<unknown_call_name>";
}

static inline kan_bool_t resolve_match_signature_at_index (struct rpl_compiler_context_t *context,
                                                           kan_interned_string_t module_name,
                                                           struct compiler_instance_expression_node_t *owner_expression,
                                                           struct compiler_instance_declaration_node_t *signature,
                                                           uint64_t signature_index,
                                                           struct resolve_expression_output_type_t *actual_type)
{
    if (signature)
    {
        if ((signature->variable.type_if_vector && signature->variable.type_if_vector != actual_type->type_if_vector) ||
            (signature->variable.type_if_matrix && signature->variable.type_if_matrix != actual_type->type_if_matrix) ||
            (signature->variable.type_if_struct && signature->variable.type_if_struct != actual_type->type_if_struct))
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Expression array item at index %ld for \"%s\" call has incorrect "
                     "type: \"%s\" while \"%s\" is expected.",
                     context->log_name, module_name, owner_expression->source_name,
                     (long) owner_expression->source_line, (long) signature_index,
                     get_expression_call_name_for_logging (owner_expression),
                     get_type_name_for_logging (actual_type->type_if_vector, actual_type->type_if_matrix,
                                                actual_type->type_if_struct),
                     get_type_name_for_logging (signature->variable.type_if_vector, signature->variable.type_if_matrix,
                                                signature->variable.type_if_struct))
            return KAN_FALSE;
        }
        else if (signature->variable.array_dimensions_count != actual_type->array_dimensions_count)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Expression array item at index %ld for \"%s\" call has incorrect "
                     "array dimension count: %ld while %ld is expected",
                     context->log_name, module_name, owner_expression->source_name,
                     (long) owner_expression->source_line, (long) signature_index,
                     get_expression_call_name_for_logging (owner_expression), actual_type->array_dimensions_count,
                     signature->variable.array_dimensions_count)
            return KAN_FALSE;
        }
        else
        {
            for (uint64_t array_dimension_index = 0u;
                 array_dimension_index < signature->variable.array_dimensions_count; ++array_dimension_index)
            {
                if (signature->variable.array_dimensions[array_dimension_index] !=
                    actual_type->array_dimensions[array_dimension_index])
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Expression array item at index %ld for \"%s\" call has "
                             "incorrect array dimension %ld size: %ld while %ld is expected",
                             context->log_name, module_name, owner_expression->source_name,
                             (long) owner_expression->source_line, (long) signature_index,
                             get_expression_call_name_for_logging (owner_expression), array_dimension_index,
                             actual_type->array_dimensions[array_dimension_index],
                             signature->variable.array_dimensions[array_dimension_index])
                    return KAN_FALSE;
                }
            }
        }
    }
    else
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Expression array does not match required \"%s\" call signature: too "
                 "many arguments.",
                 context->log_name, module_name, owner_expression->source_name, (long) owner_expression->source_line,
                 get_expression_call_name_for_logging (owner_expression))
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline kan_bool_t resolve_expression_array_with_signature (
    struct rpl_compiler_context_t *context,
    struct rpl_compiler_instance_t *instance,
    struct resolve_expression_scope_t *resolve_scope,
    struct compiler_instance_expression_node_t *target_expression,
    struct compiler_instance_expression_list_item_t **first_expression_output,
    struct kan_rpl_expression_node_t *container_expression,
    struct compiler_instance_declaration_node_t *first_argument)
{
    kan_bool_t resolved = KAN_TRUE;
    struct compiler_instance_expression_list_item_t *last_expression = NULL;
    struct resolve_expression_output_type_t output_type;
    struct compiler_instance_declaration_node_t *current_argument = first_argument;
    uint64_t current_argument_index = 0u;

    for (uint64_t index = 0u; index < container_expression->children.size; ++index)
    {
        struct compiler_instance_expression_node_t *resolved_expression;
        if (resolve_expression (context, instance, resolve_scope,
                                &((struct kan_rpl_expression_node_t *) container_expression->children.data)[index],
                                &resolved_expression, &output_type))
        {
            KAN_ASSERT (resolved_expression)
            struct compiler_instance_expression_list_item_t *list_item = kan_stack_group_allocator_allocate (
                &instance->resolve_allocator, sizeof (struct compiler_instance_expression_list_item_t),
                _Alignof (struct compiler_instance_expression_list_item_t));

            list_item->next = NULL;
            list_item->expression = resolved_expression;

            if (last_expression)
            {
                last_expression->next = list_item;
            }
            else
            {
                *first_expression_output = list_item;
            }

            last_expression = list_item;
            if (!resolve_match_signature_at_index (context, resolve_scope->function->module_name, target_expression,
                                                   current_argument, current_argument_index, &output_type))
            {
                resolved = KAN_FALSE;
            }

            current_argument = current_argument->next;
            ++current_argument_index;
        }
        else
        {
            resolved = KAN_FALSE;
        }
    }

    if (current_argument)
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Expression array does not match required \"%s\" call signature: not enough arguments.",
                 context->log_name, resolve_scope->function->module_name, container_expression->source_name,
                 (long) container_expression->source_line, get_expression_call_name_for_logging (target_expression))
        resolved = KAN_FALSE;
    }

    return resolved;
}

static struct compiler_instance_expression_node_t *resolve_find_loop_in_current_context (
    struct resolve_expression_scope_t *resolve_scope)
{
    if (resolve_scope->associated_outer_loop_if_any)
    {
        return resolve_scope->associated_outer_loop_if_any;
    }

    if (resolve_scope->parent)
    {
        return resolve_find_loop_in_current_context (resolve_scope->parent);
    }

    return NULL;
}

static kan_bool_t resolve_function_by_name (struct rpl_compiler_context_t *context,
                                            struct rpl_compiler_instance_t *instance,
                                            kan_interned_string_t function_name,
                                            enum kan_rpl_pipeline_stage_t context_stage,
                                            struct compiler_instance_function_node_t **output_node);

static struct resolve_fiend_access_linear_node_t *resolve_field_access_linearize_access_chain (
    struct rpl_compiler_context_t *context,
    kan_interned_string_t module_name,
    struct kan_rpl_expression_node_t *current_expression,
    struct kan_rpl_expression_node_t **stop_output)
{
    struct resolve_fiend_access_linear_node_t *first_node = NULL;
    while (KAN_TRUE)
    {
        struct kan_rpl_expression_node_t *input_child =
            &((struct kan_rpl_expression_node_t *) current_expression->children.data)[0u];

        struct kan_rpl_expression_node_t *field_child =
            &((struct kan_rpl_expression_node_t *) current_expression->children.data)[1u];

        if (field_child->type != KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot execute \".\" operation: right operand is not a field identifier.",
                     context->log_name, module_name, current_expression->source_name,
                     (long) current_expression->source_line)
            return NULL;
        }

        struct resolve_fiend_access_linear_node_t *new_node = kan_stack_group_allocator_allocate (
            &context->resolve_allocator, sizeof (struct resolve_fiend_access_linear_node_t),
            _Alignof (struct resolve_fiend_access_linear_node_t));

        new_node->next = first_node;
        first_node = new_node;
        new_node->field_source = field_child;

        if (input_child->type == KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION &&
            input_child->binary_operation == KAN_RPL_BINARY_OPERATION_FIELD_ACCESS)
        {
            current_expression = input_child;
        }
        else
        {
            *stop_output = input_child;
            break;
        }
    }

    return first_node;
}

static kan_bool_t is_buffer_writable_for_stage (struct compiler_instance_buffer_node_t *buffer,
                                                enum kan_rpl_pipeline_stage_t stage)
{
    switch (buffer->type)
    {
    case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
    case KAN_RPL_BUFFER_TYPE_UNIFORM:
    case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
    case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
    case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
    case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
        return KAN_FALSE;

    case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
        return stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX;

    case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
        return stage == KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t resolve_field_access_ascend_flattened_buffer (
    struct rpl_compiler_context_t *context,
    struct rpl_compiler_instance_t *instance,
    struct resolve_expression_scope_t *resolve_scope,
    uint64_t stop_expression_line,
    struct compiler_instance_buffer_node_t *buffer,
    struct resolve_fiend_access_linear_node_t *chain_first,
    struct compiler_instance_buffer_flattened_declaration_t **declaration_output,
    struct resolve_fiend_access_linear_node_t **access_resolve_next_node)
{
    KAN_ASSERT (chain_first)
    *declaration_output = NULL;
    *access_resolve_next_node = NULL;

    if (!resolve_use_buffer (context, instance, resolve_scope->function, stop_expression_line,
                             resolve_scope->function->required_stage, buffer))
    {
        return KAN_FALSE;
    }

    struct resolve_fiend_access_linear_node_t *chain_current = chain_first;
    struct compiler_instance_buffer_flattening_graph_node_t *graph_node = buffer->flattening_graph_base;

    while (chain_current)
    {
        while (graph_node)
        {
            if (graph_node->name == chain_current->field_source->identifier)
            {
                break;
            }

            graph_node = graph_node->next_on_level;
        }

        if (!graph_node)
        {
            KAN_LOG (
                rpl_compiler_context, KAN_LOG_ERROR,
                "[%s:%s:%s:%ld] Failed to resolve flattened buffer access at field \"%s\": no path for such field.",
                context->log_name, resolve_scope->function->module_name, chain_current->field_source->source_name,
                (long) chain_current->field_source->source_line, chain_current->field_source->identifier)
            return KAN_FALSE;
        }

        if (graph_node->flattened_result)
        {
            *declaration_output = graph_node->flattened_result;
            *access_resolve_next_node = chain_current->next;
            return KAN_TRUE;
        }

        chain_current = chain_current->next;
        graph_node = graph_node->first_child;
    }

    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
             "[%s:%s:%s:%ld] Failed to resolve flattened buffer access: it didn't lead to concrete leaf field.",
             context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
             (long) chain_first->field_source->source_line)
    return KAN_FALSE;
}

static inline kan_bool_t resolve_field_access_structured (struct rpl_compiler_context_t *context,
                                                          struct rpl_compiler_instance_t *instance,
                                                          struct resolve_expression_scope_t *resolve_scope,
                                                          struct compiler_instance_expression_node_t *input_node,
                                                          struct resolve_expression_output_type_t *input_node_type,
                                                          struct resolve_fiend_access_linear_node_t *chain_first,
                                                          uint64_t chain_length,
                                                          struct compiler_instance_expression_node_t *result_expression,
                                                          struct resolve_expression_output_type_t *output_type)
{
    if (input_node_type->array_dimensions_count > 0u)
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Failed to resolve structured access: attempted to use \".\" on array.",
                 context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
                 (long) chain_first->field_source->source_line)
        return KAN_FALSE;
    }

    if (input_node_type->boolean)
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Failed to resolve structured access: attempted to use \".\" on boolean.",
                 context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
                 (long) chain_first->field_source->source_line)
        return KAN_FALSE;
    }

    result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS;
    result_expression->structured_access.input = input_node;
    result_expression->structured_access.access_chain_length = chain_length;
    result_expression->structured_access.access_chain_indices = kan_stack_group_allocator_allocate (
        &instance->resolve_allocator, sizeof (uint64_t) * chain_length, _Alignof (uint64_t));

    uint64_t index = 0u;
    output_type->type_if_vector = input_node_type->type_if_vector;
    output_type->type_if_matrix = input_node_type->type_if_matrix;
    output_type->type_if_struct = input_node_type->type_if_struct;
    output_type->boolean = KAN_FALSE;
    output_type->writable = input_node_type->writable;
    output_type->array_dimensions_count = 0u;
    output_type->array_dimensions = NULL;

    if (input_node->type == COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE)
    {
        output_type->writable = is_buffer_writable_for_stage (input_node->structured_buffer_reference,
                                                              resolve_scope->function->required_stage);
    }

    struct resolve_fiend_access_linear_node_t *chain_current = chain_first;
    while (chain_current)
    {
        kan_bool_t found = KAN_FALSE;
        if (output_type->array_dimensions_count > 0u)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Failed to resolve structured access: attempted to use \".\" on array.",
                     context->log_name, resolve_scope->function->module_name, chain_first->field_source->source_name,
                     (long) chain_first->field_source->source_line)
            return KAN_FALSE;
        }

        if (output_type->type_if_vector)
        {
            if (output_type->type_if_vector->items_count == 1u)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve structured access: \"%s\" treated as scalar type and "
                         "therefore has no fields.",
                         context->log_name, resolve_scope->function->module_name,
                         chain_first->field_source->source_name, (long) chain_first->field_source->source_line,
                         output_type->type_if_vector->name)
                return KAN_FALSE;
            }

            if (chain_current->field_source->identifier[0u] != '_' ||
                chain_current->field_source->identifier[1u] < '0' ||
                chain_current->field_source->identifier[1u] > '9' ||
                chain_current->field_source->identifier[2u] != '\0')
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve structured access: vector item access specifier \".\" "
                         "doesn't match format \"_<digit>\".",
                         context->log_name, resolve_scope->function->module_name,
                         chain_first->field_source->source_name, (long) chain_first->field_source->source_line)
                return KAN_FALSE;
            }

            result_expression->structured_access.access_chain_indices[index] =
                chain_current->field_source->identifier[1u] - '0';

            if (result_expression->structured_access.access_chain_indices[index] >=
                output_type->type_if_vector->items_count)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve structured access: \"%s\" has only %ld items, but item at "
                         "index %ld requested.",
                         context->log_name, resolve_scope->function->module_name,
                         chain_first->field_source->source_name, (long) chain_first->field_source->source_line,
                         output_type->type_if_vector->name, (long) output_type->type_if_vector->items_count,
                         (long) result_expression->structured_access.access_chain_indices[index])
                return KAN_FALSE;
            }

            found = KAN_TRUE;
            switch (output_type->type_if_vector->item)
            {
            case INBUILT_TYPE_ITEM_FLOAT:
                output_type->type_if_vector = &type_f1;
                break;
            case INBUILT_TYPE_ITEM_INTEGER:
                output_type->type_if_vector = &type_i1;
                break;
            }
        }
        else if (output_type->type_if_matrix)
        {
            if (chain_current->field_source->identifier[0u] != '_' ||
                chain_current->field_source->identifier[1u] < '0' ||
                chain_current->field_source->identifier[1u] > '9' ||
                chain_current->field_source->identifier[2u] != '\0')
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve structured access: matrix column access specifier \".\" "
                         "doesn't match format \"_<digit>\".",
                         context->log_name, resolve_scope->function->module_name,
                         chain_first->field_source->source_name, (long) chain_first->field_source->source_line)
                return KAN_FALSE;
            }

            result_expression->structured_access.access_chain_indices[index] =
                chain_current->field_source->identifier[1u] - '0';

            if (result_expression->structured_access.access_chain_indices[index] >=
                output_type->type_if_matrix->columns)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve structured access: \"%s\" has only %ld columns, but column "
                         "at index %ld requested.",
                         context->log_name, resolve_scope->function->module_name,
                         chain_first->field_source->source_name, (long) chain_first->field_source->source_line,
                         output_type->type_if_matrix->name, (long) output_type->type_if_matrix->columns,
                         (long) result_expression->structured_access.access_chain_indices[index])
                return KAN_FALSE;
            }

            found = KAN_TRUE;
            switch (output_type->type_if_matrix->item)
            {
            case INBUILT_TYPE_ITEM_FLOAT:
                output_type->type_if_vector = floating_vector_types[output_type->type_if_matrix->rows - 1u];
                break;
            case INBUILT_TYPE_ITEM_INTEGER:
                output_type->type_if_vector = integer_vector_types[output_type->type_if_matrix->rows - 1u];
                break;
            }

            output_type->type_if_matrix = NULL;
        }

#define SEARCH_USING_DECLARATION                                                                                       \
    result_expression->structured_access.access_chain_indices[index] = 0u;                                             \
    while (declaration)                                                                                                \
    {                                                                                                                  \
        if (declaration->variable.name == chain_current->field_source->identifier)                                     \
        {                                                                                                              \
            found = KAN_TRUE;                                                                                          \
            output_type->type_if_vector = declaration->variable.type_if_vector;                                        \
            output_type->type_if_matrix = declaration->variable.type_if_matrix;                                        \
            output_type->type_if_struct = declaration->variable.type_if_struct;                                        \
            output_type->array_dimensions_count = declaration->variable.array_dimensions_count;                        \
            output_type->array_dimensions = declaration->variable.array_dimensions;                                    \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        ++result_expression->structured_access.access_chain_indices[index];                                            \
        declaration = declaration->next;                                                                               \
    }

        else if (output_type->type_if_struct)
        {
            struct compiler_instance_declaration_node_t *declaration = output_type->type_if_struct->first_field;
            SEARCH_USING_DECLARATION
        }
        else if (chain_current == chain_first &&
                 input_node->type == COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE)
        {
            struct compiler_instance_declaration_node_t *declaration =
                input_node->structured_buffer_reference->first_field;
            SEARCH_USING_DECLARATION
        }

#undef SEARCH_USING_DECLARATION

        if (!found)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Failed to resolve structured access at field \"%s\": no path for such field.",
                     context->log_name, resolve_scope->function->module_name, chain_current->field_source->source_name,
                     (long) chain_current->field_source->source_line, chain_current->field_source->identifier)
            return KAN_FALSE;
        }

        ++index;
        chain_current = chain_current->next;
    }

    return KAN_TRUE;
}

static inline kan_bool_t resolve_binary_operation (struct rpl_compiler_context_t *context,
                                                   struct rpl_compiler_instance_t *instance,
                                                   struct resolve_expression_scope_t *resolve_scope,
                                                   struct kan_rpl_expression_node_t *input_expression,
                                                   struct compiler_instance_expression_node_t *result_expression,
                                                   struct resolve_expression_output_type_t *output_type)
{
    // Field access parse into appropriate access operation is complicated and therefore separated from everything else.
    if (input_expression->binary_operation == KAN_RPL_BINARY_OPERATION_FIELD_ACCESS)
    {
        struct kan_rpl_expression_node_t *chain_stop_expression;
        struct resolve_fiend_access_linear_node_t *chain_first = resolve_field_access_linearize_access_chain (
            context, resolve_scope->function->module_name, input_expression, &chain_stop_expression);

        if (!chain_first)
        {
            return KAN_FALSE;
        }

        struct compiler_instance_expression_node_t *chain_input_expression = NULL;
        struct resolve_expression_output_type_t chain_input_expression_type;

        // If chain stop points to flattened buffer, we must resolve flattened buffer access first.
        if (chain_stop_expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER)
        {
            struct compiler_instance_buffer_node_t *buffer = instance->first_buffer;
            while (buffer)
            {
                if (buffer->name == chain_stop_expression->identifier)
                {
                    if (buffer->first_flattened_declaration)
                    {
                        struct compiler_instance_buffer_flattened_declaration_t *flattened_declaration;
                        if (!resolve_field_access_ascend_flattened_buffer (
                                context, instance, resolve_scope, chain_stop_expression->source_line, buffer,
                                chain_first, &flattened_declaration, &chain_first))
                        {
                            return KAN_FALSE;
                        }

                        if (chain_first)
                        {
                            chain_input_expression = kan_stack_group_allocator_allocate (
                                &instance->resolve_allocator, sizeof (struct compiler_instance_expression_node_t),
                                _Alignof (struct compiler_instance_expression_node_t));

                            chain_input_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS;
                            chain_input_expression->flattened_buffer_access = flattened_declaration;
                            chain_input_expression->module_name = resolve_scope->function->module_name;
                            chain_input_expression->source_name = chain_stop_expression->source_name;
                            chain_input_expression->source_line = chain_stop_expression->source_line;

                            chain_input_expression_type.type_if_vector =
                                flattened_declaration->source_declaration->variable.type_if_vector;
                            chain_input_expression_type.type_if_matrix =
                                flattened_declaration->source_declaration->variable.type_if_matrix;
                            chain_input_expression_type.type_if_struct =
                                flattened_declaration->source_declaration->variable.type_if_struct;
                            chain_input_expression_type.boolean = KAN_FALSE;
                            chain_input_expression_type.writable =
                                is_buffer_writable_for_stage (buffer, resolve_scope->function->required_stage);
                            chain_input_expression_type.array_dimensions_count =
                                flattened_declaration->source_declaration->variable.array_dimensions_count;
                            chain_input_expression_type.array_dimensions =
                                flattened_declaration->source_declaration->variable.array_dimensions;
                        }
                        else
                        {
                            // Full access chain was resolved as flattened access.
                            result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS;
                            result_expression->flattened_buffer_access = flattened_declaration;
                            output_type->type_if_vector =
                                flattened_declaration->source_declaration->variable.type_if_vector;
                            output_type->type_if_matrix =
                                flattened_declaration->source_declaration->variable.type_if_matrix;
                            output_type->type_if_struct =
                                flattened_declaration->source_declaration->variable.type_if_struct;
                            output_type->boolean = KAN_FALSE;
                            output_type->writable =
                                is_buffer_writable_for_stage (buffer, resolve_scope->function->required_stage);
                            output_type->array_dimensions_count =
                                flattened_declaration->source_declaration->variable.array_dimensions_count;
                            output_type->array_dimensions =
                                flattened_declaration->source_declaration->variable.array_dimensions;
                            return KAN_TRUE;
                        }
                    }

                    break;
                }

                buffer = buffer->next;
            }
        }

        if (!chain_input_expression && !resolve_expression (context, instance, resolve_scope, chain_stop_expression,
                                                            &chain_input_expression, &chain_input_expression_type))
        {
            return KAN_FALSE;
        }

        uint64_t chain_length = 0u;
        struct resolve_fiend_access_linear_node_t *chain_item = chain_first;

        while (chain_item)
        {
            ++chain_length;
            chain_item = chain_item->next;
        }

        return resolve_field_access_structured (context, instance, resolve_scope, chain_input_expression,
                                                &chain_input_expression_type, chain_first, chain_length,
                                                result_expression, output_type);
    }

    struct resolve_expression_output_type_t left_operand_type;
    if (!resolve_expression (context, instance, resolve_scope,
                             &((struct kan_rpl_expression_node_t *) input_expression->children.data)[0u],
                             &result_expression->binary_operation.left_operand, &left_operand_type))
    {
        return KAN_FALSE;
    }

    result_expression->binary_operation.left_type_if_vector = left_operand_type.type_if_vector;
    result_expression->binary_operation.left_type_if_matrix = left_operand_type.type_if_matrix;
    result_expression->binary_operation.left_type_if_struct = left_operand_type.type_if_struct;

    struct resolve_expression_output_type_t right_operand_type;
    if (!resolve_expression (context, instance, resolve_scope,
                             &((struct kan_rpl_expression_node_t *) input_expression->children.data)[1u],
                             &result_expression->binary_operation.right_operand, &right_operand_type))
    {
        return KAN_FALSE;
    }

    result_expression->binary_operation.right_type_if_vector = right_operand_type.type_if_vector;
    result_expression->binary_operation.right_type_if_matrix = right_operand_type.type_if_matrix;
    result_expression->binary_operation.right_type_if_struct = right_operand_type.type_if_struct;

    switch (input_expression->binary_operation)
    {
    case KAN_RPL_BINARY_OPERATION_FIELD_ACCESS:
        // Should be processed separately in upper segment.
        KAN_ASSERT (KAN_FALSE)
        return KAN_FALSE;

    case KAN_RPL_BINARY_OPERATION_ARRAY_ACCESS:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ARRAY_INDEX;
        if (left_operand_type.array_dimensions_count == 0u)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot execute array access as left operand in not an array.", context->log_name,
                     resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line)
            return KAN_FALSE;
        }

        if (right_operand_type.type_if_vector != &type_i1)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot execute array access as right operand is \"%s\" instead of i1.",
                     context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line,
                     get_type_name_for_logging (right_operand_type.type_if_vector, right_operand_type.type_if_matrix,
                                                right_operand_type.type_if_struct))
            return KAN_FALSE;
        }

        output_type->type_if_vector = left_operand_type.type_if_vector;
        output_type->type_if_matrix = left_operand_type.type_if_matrix;
        output_type->type_if_struct = left_operand_type.type_if_struct;
        output_type->boolean = left_operand_type.boolean;
        output_type->writable = left_operand_type.writable;
        output_type->array_dimensions_count = left_operand_type.array_dimensions_count - 1u;
        output_type->array_dimensions = left_operand_type.array_dimensions + 1u;
        return KAN_TRUE;

#define CANNOT_EXECUTE_ON_ARRAYS(OPERATOR_STRING)                                                                      \
    if (left_operand_type.array_dimensions_count != 0u || right_operand_type.array_dimensions_count != 0u)             \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATOR_STRING "\" operation on arrays.", context->log_name,      \
                 resolve_scope->function->module_name, input_expression->source_name,                                  \
                 (long) input_expression->source_line)                                                                 \
        return KAN_FALSE;                                                                                              \
    }

#define CAN_ONLY_EXECUTE_ON_MATCHING_BUILTIN(OPERATOR_STRING)                                                          \
    if (left_operand_type.type_if_vector != right_operand_type.type_if_vector ||                                       \
        left_operand_type.type_if_matrix != right_operand_type.type_if_matrix || left_operand_type.type_if_struct ||   \
        right_operand_type.type_if_struct)                                                                             \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATOR_STRING "\" on \"%s\" and \"%s\".", context->log_name,     \
                 resolve_scope->function->module_name, input_expression->source_name,                                  \
                 (long) input_expression->source_line,                                                                 \
                 get_type_name_for_logging (left_operand_type.type_if_vector, left_operand_type.type_if_matrix,        \
                                            left_operand_type.type_if_struct),                                         \
                 get_type_name_for_logging (right_operand_type.type_if_vector, right_operand_type.type_if_matrix,      \
                                            right_operand_type.type_if_struct))                                        \
        return KAN_FALSE;                                                                                              \
    }

#define COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION                                                                    \
    output_type->type_if_vector = left_operand_type.type_if_vector;                                                    \
    output_type->type_if_matrix = left_operand_type.type_if_matrix;                                                    \
    output_type->type_if_struct = left_operand_type.type_if_struct;                                                    \
    output_type->boolean = left_operand_type.boolean;                                                                  \
    output_type->writable = KAN_FALSE;                                                                                 \
    output_type->array_dimensions_count = 0u;                                                                          \
    output_type->array_dimensions = NULL

#define COPY_TYPE_FROM_RIGHT_FOR_ELEMENTAL_OPERATION                                                                   \
    output_type->type_if_vector = right_operand_type.type_if_vector;                                                   \
    output_type->type_if_matrix = right_operand_type.type_if_matrix;                                                   \
    output_type->type_if_struct = right_operand_type.type_if_struct;                                                   \
    output_type->boolean = right_operand_type.boolean;                                                                 \
    output_type->writable = KAN_FALSE;                                                                                 \
    output_type->array_dimensions_count = 0u;                                                                          \
    output_type->array_dimensions = NULL

    case KAN_RPL_BINARY_OPERATION_ADD:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ADD;
        CANNOT_EXECUTE_ON_ARRAYS ("+")
        CAN_ONLY_EXECUTE_ON_MATCHING_BUILTIN ("+")
        COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_SUBTRACT:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_SUBTRACT;
        CANNOT_EXECUTE_ON_ARRAYS ("-")
        CAN_ONLY_EXECUTE_ON_MATCHING_BUILTIN ("-")
        COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_MULTIPLY:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_MULTIPLY;
        CANNOT_EXECUTE_ON_ARRAYS ("*")

        // Multiply vectors by elements.
        if (left_operand_type.type_if_vector && left_operand_type.type_if_vector == right_operand_type.type_if_vector)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Multiply vector by scalar of the same type.
        if (left_operand_type.type_if_vector && right_operand_type.type_if_vector &&
            left_operand_type.type_if_vector->item == right_operand_type.type_if_vector->item &&
            right_operand_type.type_if_vector->items_count == 1u)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Multiply matrix by scalar of the same type.
        if (left_operand_type.type_if_matrix && right_operand_type.type_if_vector &&
            left_operand_type.type_if_matrix->item == right_operand_type.type_if_vector->item &&
            right_operand_type.type_if_vector->items_count == 1u)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Multiply matrix by vector of the same type.
        if (left_operand_type.type_if_matrix && right_operand_type.type_if_vector &&
            left_operand_type.type_if_matrix->item == right_operand_type.type_if_vector->item &&
            left_operand_type.type_if_matrix->columns == right_operand_type.type_if_vector->items_count)
        {
            COPY_TYPE_FROM_RIGHT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Multiply vector by matrix of the same type.
        if (left_operand_type.type_if_vector && right_operand_type.type_if_matrix &&
            left_operand_type.type_if_vector->item == right_operand_type.type_if_matrix->item &&
            left_operand_type.type_if_vector->items_count == right_operand_type.type_if_matrix->rows)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Multiply matrix by matrix of the same type.
        if (left_operand_type.type_if_matrix && right_operand_type.type_if_matrix &&
            left_operand_type.type_if_matrix->item == right_operand_type.type_if_matrix->item &&
            left_operand_type.type_if_matrix->columns == right_operand_type.type_if_matrix->rows)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Cannot execute \"*\" on \"%s\" and \"%s\".",
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                 (long) input_expression->source_line,
                 get_type_name_for_logging (left_operand_type.type_if_vector, left_operand_type.type_if_matrix,
                                            left_operand_type.type_if_struct),
                 get_type_name_for_logging (right_operand_type.type_if_vector, right_operand_type.type_if_matrix,
                                            right_operand_type.type_if_struct))
        return KAN_FALSE;

    case KAN_RPL_BINARY_OPERATION_DIVIDE:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_DIVIDE;
        CANNOT_EXECUTE_ON_ARRAYS ("/")

        // Divide vectors of the same type.
        if (left_operand_type.type_if_vector && left_operand_type.type_if_vector == right_operand_type.type_if_vector)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        // Divide vector by scalar of the same type.
        if (left_operand_type.type_if_vector && right_operand_type.type_if_vector &&
            left_operand_type.type_if_vector->item == right_operand_type.type_if_vector->item &&
            right_operand_type.type_if_vector->items_count == 1u)
        {
            COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Cannot execute \"/\" on \"%s\" and \"%s\".",
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                 (long) input_expression->source_line,
                 get_type_name_for_logging (left_operand_type.type_if_vector, left_operand_type.type_if_matrix,
                                            left_operand_type.type_if_struct),
                 get_type_name_for_logging (right_operand_type.type_if_vector, right_operand_type.type_if_matrix,
                                            right_operand_type.type_if_struct))
        return KAN_FALSE;

#define INTEGER_ONLY_VECTOR_OPERATION(OPERATION_STRING)                                                                \
    CANNOT_EXECUTE_ON_ARRAYS (OPERATION_STRING)                                                                        \
                                                                                                                       \
    if (!left_operand_type.type_if_vector || !right_operand_type.type_if_vector ||                                     \
        left_operand_type.type_if_vector->item != INBUILT_TYPE_ITEM_INTEGER ||                                         \
        right_operand_type.type_if_vector->item != INBUILT_TYPE_ITEM_INTEGER)                                          \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATION_STRING                                                   \
                 "\" on \"%s\" and \"%s\", only integer vectors are supported.",                                       \
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,               \
                 (long) input_expression->source_line,                                                                 \
                 get_type_name_for_logging (left_operand_type.type_if_vector, left_operand_type.type_if_matrix,        \
                                            left_operand_type.type_if_struct),                                         \
                 get_type_name_for_logging (right_operand_type.type_if_vector, right_operand_type.type_if_matrix,      \
                                            right_operand_type.type_if_struct))                                        \
    }                                                                                                                  \
                                                                                                                       \
    COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION

    case KAN_RPL_BINARY_OPERATION_MODULUS:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_MODULUS;
        INTEGER_ONLY_VECTOR_OPERATION ("%%");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_ASSIGN:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_ASSIGN;
        CANNOT_EXECUTE_ON_ARRAYS ("=")

        if (!left_operand_type.writable)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot execute \"=\" as its output is not writable.", context->log_name,
                     resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line)
            return KAN_FALSE;
        }

        if (left_operand_type.type_if_vector != right_operand_type.type_if_vector ||
            left_operand_type.type_if_matrix != right_operand_type.type_if_matrix ||
            left_operand_type.type_if_struct != right_operand_type.type_if_struct)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Cannot execute \"=\" on \"%s\" and \"%s\".",
                     context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line,
                     get_type_name_for_logging (left_operand_type.type_if_vector, left_operand_type.type_if_matrix,
                                                left_operand_type.type_if_struct),
                     get_type_name_for_logging (right_operand_type.type_if_vector, right_operand_type.type_if_matrix,
                                                right_operand_type.type_if_struct))
            return KAN_FALSE;
        }

        COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION;
        return KAN_TRUE;

#define LOGIC_OPERATION(OPERATION_STRING)                                                                              \
    CANNOT_EXECUTE_ON_ARRAYS (OPERATION_STRING)                                                                        \
    if (!left_operand_type.boolean || !right_operand_type.boolean)                                                     \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATION_STRING                                                   \
                 "\" on \"%s\" and \"%s\", only booleans are supported.",                                              \
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,               \
                 (long) input_expression->source_line,                                                                 \
                 get_type_name_for_logging (left_operand_type.type_if_vector, left_operand_type.type_if_matrix,        \
                                            left_operand_type.type_if_struct),                                         \
                 get_type_name_for_logging (right_operand_type.type_if_vector, right_operand_type.type_if_matrix,      \
                                            right_operand_type.type_if_struct))                                        \
        return KAN_FALSE;                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    output_type->type_if_vector = NULL;                                                                                \
    output_type->type_if_matrix = NULL;                                                                                \
    output_type->type_if_struct = NULL;                                                                                \
    output_type->boolean = KAN_TRUE;                                                                                   \
    output_type->writable = KAN_FALSE;                                                                                 \
    output_type->array_dimensions_count = 0u;                                                                          \
    output_type->array_dimensions = NULL

    case KAN_RPL_BINARY_OPERATION_AND:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_AND;
        LOGIC_OPERATION ("&&");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_OR:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_OR;
        LOGIC_OPERATION ("||");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_EQUAL:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_EQUAL;
        INTEGER_ONLY_VECTOR_OPERATION ("==");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_NOT_EQUAL:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NOT_EQUAL;
        INTEGER_ONLY_VECTOR_OPERATION ("!=");
        return KAN_TRUE;

#define SCALAR_ONLY_OPERATION(OPERATION_STRING)                                                                        \
    CANNOT_EXECUTE_ON_ARRAYS (OPERATION_STRING)                                                                        \
                                                                                                                       \
    if (!left_operand_type.type_if_vector || !right_operand_type.type_if_vector ||                                     \
        left_operand_type.type_if_vector->items_count > 1u || right_operand_type.type_if_vector->items_count > 1u)     \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Cannot execute \"" OPERATION_STRING                                                   \
                 "\" on \"%s\" and \"%s\", only one-item vectors are supported.",                                      \
                 context->log_name, resolve_scope->function->module_name, input_expression->source_name,               \
                 (long) input_expression->source_line,                                                                 \
                 get_type_name_for_logging (left_operand_type.type_if_vector, left_operand_type.type_if_matrix,        \
                                            left_operand_type.type_if_struct),                                         \
                 get_type_name_for_logging (right_operand_type.type_if_vector, right_operand_type.type_if_matrix,      \
                                            right_operand_type.type_if_struct))                                        \
    }                                                                                                                  \
                                                                                                                       \
    COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION

    case KAN_RPL_BINARY_OPERATION_LESS:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_LESS;
        SCALAR_ONLY_OPERATION ("<");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_GREATER:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_GREATER;
        SCALAR_ONLY_OPERATION (">");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_LESS_OR_EQUAL:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_LESS_OR_EQUAL;
        SCALAR_ONLY_OPERATION ("<=");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_GREATER_OR_EQUAL:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_GREATER_OR_EQUAL;
        SCALAR_ONLY_OPERATION (">=");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_BITWISE_AND:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_AND;
        INTEGER_ONLY_VECTOR_OPERATION ("&");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_BITWISE_OR:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_OR;
        INTEGER_ONLY_VECTOR_OPERATION ("|");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_BITWISE_XOR:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_XOR;
        INTEGER_ONLY_VECTOR_OPERATION ("|");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_BITWISE_LSHIFT:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_LEFT_SHIFT;
        INTEGER_ONLY_VECTOR_OPERATION ("|");
        return KAN_TRUE;

    case KAN_RPL_BINARY_OPERATION_BITWISE_RSHIFT:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_RIGHT_SHIFT;
        INTEGER_ONLY_VECTOR_OPERATION ("|");
        return KAN_TRUE;

#undef CANNOT_EXECUTE_ON_ARRAYS
#undef CAN_ONLY_EXECUTE_ON_MATCHING_BUILTIN
#undef COPY_TYPE_FROM_LEFT_FOR_ELEMENTAL_OPERATION
#undef COPY_TYPE_FROM_RIGHT_FOR_ELEMENTAL_OPERATION
#undef INTEGER_ONLY_VECTOR_OPERATION
#undef LOGIC_OPERATION
#undef SCALAR_ONLY_OPERATION
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t resolve_unary_operation (struct rpl_compiler_context_t *context,
                                                  struct rpl_compiler_instance_t *instance,
                                                  struct resolve_expression_scope_t *resolve_scope,
                                                  struct kan_rpl_expression_node_t *input_expression,
                                                  struct compiler_instance_expression_node_t *result_expression,
                                                  struct resolve_expression_output_type_t *output_type)
{
    struct resolve_expression_output_type_t operand_type;
    if (!resolve_expression (context, instance, resolve_scope,
                             &((struct kan_rpl_expression_node_t *) input_expression->children.data)[0u],
                             &result_expression->unary_operation.operand, &operand_type))
    {
        return KAN_FALSE;
    }

    result_expression->unary_operation.type_if_vector = operand_type.type_if_vector;
    result_expression->unary_operation.type_if_matrix = operand_type.type_if_matrix;
    result_expression->unary_operation.type_if_struct = operand_type.type_if_struct;

    switch (input_expression->unary_operation)
    {
    case KAN_RPL_UNARY_OPERATION_NEGATE:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NEGATE;
        if (!operand_type.type_if_vector && !operand_type.type_if_matrix)
        {
            KAN_LOG (
                rpl_compiler_context, KAN_LOG_ERROR,
                "[%s:%s:%s:%ld] Cannot apply \"~\" operation to type \"%s\", only vectors and matrices are supported.",
                context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                (long) input_expression->source_line,
                get_type_name_for_logging (operand_type.type_if_vector, operand_type.type_if_matrix,
                                           operand_type.type_if_struct))
            return KAN_FALSE;
        }

        output_type->type_if_vector = operand_type.type_if_vector;
        output_type->type_if_matrix = operand_type.type_if_matrix;
        return KAN_TRUE;

    case KAN_RPL_UNARY_OPERATION_NOT:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_NOT;
        if (!operand_type.boolean)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot apply \"!\" operation to non-boolean type \"%s\".", context->log_name,
                     resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line,
                     get_type_name_for_logging (operand_type.type_if_vector, operand_type.type_if_matrix,
                                                operand_type.type_if_struct))
            return KAN_FALSE;
        }

        output_type->boolean = KAN_TRUE;
        return KAN_TRUE;

    case KAN_RPL_UNARY_OPERATION_BITWISE_NOT:
        result_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_OPERATION_BITWISE_NOT;
        if (operand_type.type_if_vector != &type_i1)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Cannot apply \"~\" operation to type \"%s\", only i1 is supported.",
                     context->log_name, resolve_scope->function->module_name, input_expression->source_name,
                     (long) input_expression->source_line,
                     get_type_name_for_logging (operand_type.type_if_vector, operand_type.type_if_matrix,
                                                operand_type.type_if_struct))
            return KAN_FALSE;
        }

        output_type->type_if_vector = &type_i1;
        return KAN_TRUE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static kan_bool_t resolve_expression (struct rpl_compiler_context_t *context,
                                      struct rpl_compiler_instance_t *instance,
                                      struct resolve_expression_scope_t *resolve_scope,
                                      struct kan_rpl_expression_node_t *expression,
                                      struct compiler_instance_expression_node_t **output,
                                      struct resolve_expression_output_type_t *output_type)
{
    *output = NULL;
    output_type->type_if_vector = NULL;
    output_type->type_if_matrix = NULL;
    output_type->type_if_struct = NULL;
    output_type->array_dimensions_count = 0u;
    output_type->array_dimensions = NULL;
    output_type->boolean = KAN_FALSE;
    output_type->writable = KAN_FALSE;

    if (expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    {
        return KAN_TRUE;
    }
    // We check conditional expressions before anything else as they have special allocation strategy.
    else if (expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE)
    {
        struct resolve_expression_output_type_t output_type_mute;
        switch (evaluate_conditional (context, resolve_scope->function->module_name,
                                      &((struct kan_rpl_expression_node_t *) expression->children.data)[0u], KAN_TRUE))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            return KAN_FALSE;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
            return resolve_expression (context, instance, resolve_scope,
                                       &((struct kan_rpl_expression_node_t *) expression->children.data)[1u], output,
                                       &output_type_mute);

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            return KAN_TRUE;
        }
    }
    else if (expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS)
    {
        switch (evaluate_conditional (context, resolve_scope->function->module_name,
                                      &((struct kan_rpl_expression_node_t *) expression->children.data)[0u], KAN_TRUE))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            return KAN_FALSE;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
        {
            if (!check_alias_or_variable_name_is_not_occupied (instance, resolve_scope, expression->alias_name))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to add alias \"%s\" as its name is already occupied by other active "
                         "alias in this scope.",
                         context->log_name, resolve_scope->function->module_name, expression->source_name,
                         (long) expression->source_line, expression->alias_name)
                return KAN_FALSE;
            }

            struct resolve_expression_alias_node_t *alias_node = kan_stack_group_allocator_allocate (
                &context->resolve_allocator, sizeof (struct resolve_expression_alias_node_t),
                _Alignof (struct resolve_expression_alias_node_t));

            alias_node->name = expression->alias_name;
            if (!resolve_expression (context, instance, resolve_scope,
                                     &((struct kan_rpl_expression_node_t *) expression->children.data)[1u],
                                     &alias_node->resolved_expression, &alias_node->resolved_output_type))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Failed to resolve alias \"%s\" internal expression.", context->log_name,
                         resolve_scope->function->module_name, expression->source_name, (long) expression->source_line,
                         expression->alias_name)
                return KAN_FALSE;
            }

            alias_node->next = resolve_scope->first_alias;
            resolve_scope->first_alias = alias_node;
            return KAN_TRUE;
        }

        case CONDITIONAL_EVALUATION_RESULT_FALSE:
            return KAN_TRUE;
        }
    }
    // If it is an alias: pre-resolve it without creating excessive nodes.
    else if (expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER)
    {
        struct resolve_expression_alias_node_t *alias = resolve_find_alias (resolve_scope, expression->identifier);
        if (alias)
        {
            *output = alias->resolved_expression;
            output_type->type_if_vector = alias->resolved_output_type.type_if_vector;
            output_type->type_if_matrix = alias->resolved_output_type.type_if_matrix;
            output_type->type_if_struct = alias->resolved_output_type.type_if_struct;
            output_type->boolean = alias->resolved_output_type.boolean;
            output_type->writable = alias->resolved_output_type.writable;
            output_type->array_dimensions_count = alias->resolved_output_type.array_dimensions_count;
            output_type->array_dimensions = alias->resolved_output_type.array_dimensions;
            return KAN_TRUE;
        }
    }

    struct compiler_instance_expression_node_t *new_expression = kan_stack_group_allocator_allocate (
        &instance->resolve_allocator, sizeof (struct compiler_instance_expression_node_t),
        _Alignof (struct compiler_instance_expression_node_t));

    new_expression->module_name = resolve_scope->function->module_name;
    new_expression->source_name = expression->source_name;
    new_expression->source_line = expression->source_line;
    *output = new_expression;

    switch (expression->type)
    {
    case KAN_RPL_EXPRESSION_NODE_TYPE_NOPE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE:
    case KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_ALIAS:
        // Should've been processed earlier.
        KAN_ASSERT (KAN_FALSE)
        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_IDENTIFIER:
    {
        struct compiler_instance_buffer_node_t *buffer = instance->first_buffer;
        while (buffer)
        {
            if (buffer->name == expression->identifier)
            {
                if (buffer->first_flattened_declaration)
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Caught attempt to access flat buffer \"%s\" without selecting its field. "
                             "Flat buffers can not be accessed themselves as they're only containers for technically "
                             "separated data.",
                             context->log_name, resolve_scope->function->module_name, expression->source_name,
                             (long) expression->source_line, expression->identifier)
                    return KAN_FALSE;
                }

                new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE;
                new_expression->structured_buffer_reference = buffer;
                return resolve_use_buffer (context, instance, resolve_scope->function, expression->source_line,
                                           resolve_scope->function->required_stage, buffer);
            }

            buffer = buffer->next;
        }

        struct compiler_instance_scope_variable_item_t *variable =
            resolve_find_variable (resolve_scope, expression->identifier);

        if (variable)
        {
            new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_REFERENCE;
            new_expression->variable_reference = variable;

            output_type->type_if_vector = variable->variable->type_if_vector;
            output_type->type_if_matrix = variable->variable->type_if_matrix;
            output_type->type_if_struct = variable->variable->type_if_struct;
            output_type->array_dimensions_count = variable->variable->array_dimensions_count;
            output_type->array_dimensions = variable->variable->array_dimensions;
            output_type->boolean = KAN_FALSE;
            output_type->writable = KAN_TRUE;
            return KAN_TRUE;
        }

        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Cannot resolve identifier \"%s\" to either variable or structured buffer access.",
                 context->log_name, resolve_scope->function->module_name, expression->source_name,
                 (long) expression->source_line, expression->identifier)
        return KAN_FALSE;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_INTEGER_LITERAL:
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_INTEGER_LITERAL;
        new_expression->integer_literal = expression->integer_literal;
        output_type->type_if_vector = &type_i1;
        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL:
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_FLOATING_LITERAL;
        new_expression->floating_literal = expression->floating_literal;
        output_type->type_if_vector = &type_f1;
        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_DECLARATION;
        kan_bool_t resolved = KAN_TRUE;

        if (!check_alias_or_variable_name_is_not_occupied (instance, resolve_scope,
                                                           expression->variable_declaration.variable_name))
        {
            resolved = KAN_FALSE;
        }

        new_expression->variable_declaration.name = expression->variable_declaration.variable_name;
        new_expression->variable_declaration.array_dimensions_count = expression->children.size;

        if (!resolve_variable_type (context, instance, new_expression->module_name,
                                    &new_expression->variable_declaration, expression->variable_declaration.type_name,
                                    expression->variable_declaration.variable_name, expression->source_name,
                                    expression->source_line))
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_array_dimensions (context, instance, new_expression->module_name,
                                       &new_expression->variable_declaration, &expression->children, KAN_TRUE))
        {
            resolved = KAN_FALSE;
        }

        if (resolved)
        {
            struct resolve_expression_scope_t *owner_scope = resolve_scope;
            while (owner_scope && !owner_scope->associated_resolved_scope_if_any)
            {
                owner_scope = owner_scope->parent;
            }

            if (owner_scope)
            {
                struct compiler_instance_scope_variable_item_t *item = kan_stack_group_allocator_allocate (
                    &instance->resolve_allocator, sizeof (struct compiler_instance_scope_variable_item_t),
                    _Alignof (struct compiler_instance_scope_variable_item_t));

                item->variable = &new_expression->variable_declaration;
                item->next = owner_scope->associated_resolved_scope_if_any->scope.first_variable;
                owner_scope->associated_resolved_scope_if_any->scope.first_variable = item;
            }
            else
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Internal error: unable to register declared variable \"%s\" as there is no "
                         "suitable scope found.",
                         context->log_name, resolve_scope->function->module_name, expression->source_name,
                         (long) expression->source_line, expression->variable_declaration.variable_name)
                resolved = KAN_FALSE;
            }

            output_type->type_if_vector = new_expression->variable_declaration.type_if_vector;
            output_type->type_if_matrix = new_expression->variable_declaration.type_if_matrix;
            output_type->type_if_struct = new_expression->variable_declaration.type_if_struct;
            output_type->array_dimensions_count = new_expression->variable_declaration.array_dimensions_count;
            output_type->array_dimensions = new_expression->variable_declaration.array_dimensions;
            output_type->boolean = KAN_FALSE;
            output_type->writable = KAN_TRUE;
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION:
        return resolve_binary_operation (context, instance, resolve_scope, expression, new_expression, output_type);

    case KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION:
        return resolve_unary_operation (context, instance, resolve_scope, expression, new_expression, output_type);

    case KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_SCOPE;
        new_expression->scope.first_variable = NULL;
        new_expression->scope.first_expression = NULL;

        struct resolve_expression_scope_t child_scope = {
            .parent = resolve_scope,
            .function = resolve_scope->function,
            .first_alias = NULL,
            .associated_resolved_scope_if_any = new_expression,
            .associated_outer_loop_if_any = NULL,
        };

        kan_bool_t resolved = KAN_TRUE;
        struct compiler_instance_expression_list_item_t *last_expression = NULL;
        struct resolve_expression_output_type_t internal_output_type;

        for (uint64_t index = 0u; index < expression->children.size; ++index)
        {
            struct compiler_instance_expression_node_t *resolved_expression;
            if (resolve_expression (context, instance, &child_scope,
                                    &((struct kan_rpl_expression_node_t *) expression->children.data)[index],
                                    &resolved_expression, &internal_output_type))
            {
                // Expression will be null for inactive conditionals and for conditional aliases.
                if (resolved_expression)
                {
                    struct compiler_instance_expression_list_item_t *list_item = kan_stack_group_allocator_allocate (
                        &instance->resolve_allocator, sizeof (struct compiler_instance_expression_list_item_t),
                        _Alignof (struct compiler_instance_expression_list_item_t));

                    list_item->next = NULL;
                    list_item->expression = resolved_expression;

                    if (last_expression)
                    {
                        last_expression->next = list_item;
                    }
                    else
                    {
                        new_expression->scope.first_expression = list_item;
                    }

                    last_expression = list_item;
                }
            }
            else
            {
                resolved = KAN_FALSE;
            }
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL:
    {
        struct compiler_instance_sampler_node_t *sampler = instance->first_sampler;
        while (sampler)
        {
            if (sampler->name == expression->function_name)
            {
                new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_SAMPLER_CALL;
                new_expression->sampler_call.sampler = sampler;
                kan_bool_t resolved = KAN_TRUE;

                if (!resolve_use_sampler (instance, resolve_scope->function, sampler))
                {
                    resolved = KAN_FALSE;
                }

                struct compiler_instance_declaration_node_t *signature_first_element;
                switch (sampler->type)
                {
                case KAN_RPL_SAMPLER_TYPE_2D:
                    signature_first_element = sampler_2d_call_signature_first_element;
                    break;
                }

                if (!resolve_expression_array_with_signature (context, instance, resolve_scope, new_expression,
                                                              &new_expression->sampler_call.first_argument, expression,
                                                              signature_first_element))
                {
                    resolved = KAN_FALSE;
                }

                output_type->type_if_vector = &type_f4;
                return resolved;
            }

            sampler = sampler->next;
        }

        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_FUNCTION_CALL;
        kan_bool_t resolved = KAN_TRUE;
        new_expression->function_call.function = NULL;

        if (!resolve_function_by_name (context, instance, expression->function_name,
                                       resolve_scope->function->required_stage,
                                       &new_expression->function_call.function))
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_expression_array_with_signature (context, instance, resolve_scope, new_expression,
                                                      &new_expression->function_call.first_argument, expression,
                                                      new_expression->function_call.function->first_argument))
        {
            resolved = KAN_FALSE;
        }

        output_type->type_if_vector = new_expression->function_call.function->return_type_if_vector;
        output_type->type_if_matrix = new_expression->function_call.function->return_type_if_matrix;
        output_type->type_if_struct = new_expression->function_call.function->return_type_if_struct;
        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_CONSTRUCTOR;
        kan_bool_t resolved = KAN_TRUE;
        new_expression->constructor.type_if_vector = NULL;
        new_expression->constructor.type_if_matrix = NULL;
        new_expression->constructor.type_if_struct = NULL;

        if (!(new_expression->constructor.type_if_vector =
                  find_inbuilt_vector_type (expression->constructor_type_name)) &&
            !(new_expression->constructor.type_if_matrix =
                  find_inbuilt_matrix_type (expression->constructor_type_name)) &&
            !resolve_use_struct (context, instance, expression->constructor_type_name,
                                 &new_expression->constructor.type_if_struct))
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Constructor \"%s\" type is unknown.",
                     context->log_name, new_expression->module_name, new_expression->source_name,
                     (long) new_expression->source_line, expression->constructor_type_name)
            resolved = KAN_FALSE;
        }

        if (resolved)
        {
            struct compiler_instance_declaration_node_t *signature = NULL;
            if (new_expression->constructor.type_if_vector)
            {
                signature = new_expression->constructor.type_if_vector->constructor_signature;
            }
            else if (new_expression->constructor.type_if_matrix)
            {
                signature = new_expression->constructor.type_if_matrix->constructor_signature;
            }
            else if (new_expression->constructor.type_if_struct)
            {
                signature = new_expression->constructor.type_if_struct->first_field;
            }

            if (!resolve_expression_array_with_signature (context, instance, resolve_scope, new_expression,
                                                          &new_expression->constructor.first_argument, expression,
                                                          signature))
            {
                resolved = KAN_FALSE;
            }
        }

        output_type->type_if_vector = new_expression->constructor.type_if_vector;
        output_type->type_if_matrix = new_expression->constructor.type_if_matrix;
        output_type->type_if_struct = new_expression->constructor.type_if_struct;
        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_IF:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_IF;
        kan_bool_t resolved = KAN_TRUE;
        struct resolve_expression_output_type_t internal_output_type;

        if (resolve_expression (context, instance, resolve_scope,
                                &((struct kan_rpl_expression_node_t *) expression->children.data)[0u],
                                &new_expression->if_.condition, &internal_output_type))
        {
            if (!internal_output_type.boolean)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Condition of if cannot be resolved as boolean.", context->log_name,
                         new_expression->module_name, new_expression->source_name, (long) new_expression->source_line)
                resolved = KAN_FALSE;
            }
        }
        else
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_expression (context, instance, resolve_scope,
                                 &((struct kan_rpl_expression_node_t *) expression->children.data)[1u],
                                 &new_expression->if_.when_true, &internal_output_type))
        {
            resolved = KAN_FALSE;
        }

        if (expression->children.size == 3u)
        {
            if (!resolve_expression (context, instance, resolve_scope,
                                     &((struct kan_rpl_expression_node_t *) expression->children.data)[2u],
                                     &new_expression->if_.when_false, &internal_output_type))
            {
                resolved = KAN_FALSE;
            }
        }
        else
        {
            new_expression->if_.when_false = NULL;
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FOR:
    {
        // Loop must be inside scope to avoid leaking out init variable.
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_SCOPE;
        new_expression->scope.first_variable = NULL;

        struct compiler_instance_expression_node_t *loop_expression = kan_stack_group_allocator_allocate (
            &instance->resolve_allocator, sizeof (struct compiler_instance_expression_node_t),
            _Alignof (struct compiler_instance_expression_node_t));

        loop_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_FOR;
        loop_expression->module_name = resolve_scope->function->module_name;
        loop_expression->source_name = expression->source_name;
        loop_expression->source_line = expression->source_line;

        struct compiler_instance_expression_list_item_t *list_item = kan_stack_group_allocator_allocate (
            &instance->resolve_allocator, sizeof (struct compiler_instance_expression_list_item_t),
            _Alignof (struct compiler_instance_expression_list_item_t));

        list_item->next = NULL;
        list_item->expression = loop_expression;
        new_expression->scope.first_expression = list_item;

        kan_bool_t resolved = KAN_TRUE;
        struct resolve_expression_output_type_t internal_output_type;

        struct resolve_expression_scope_t loop_init_scope = {
            .parent = resolve_scope,
            .function = resolve_scope->function,
            .first_alias = NULL,
            .associated_resolved_scope_if_any = new_expression,
            .associated_outer_loop_if_any = loop_expression,
        };

        if (!resolve_expression (context, instance, &loop_init_scope,
                                 &((struct kan_rpl_expression_node_t *) expression->children.data)[0u],
                                 &loop_expression->for_.init, &internal_output_type))
        {
            resolved = KAN_FALSE;
        }

        if (resolve_expression (context, instance, &loop_init_scope,
                                &((struct kan_rpl_expression_node_t *) expression->children.data)[1u],
                                &loop_expression->for_.condition, &internal_output_type))
        {
            if (!internal_output_type.boolean)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Condition of for cannot be resolved as boolean.", context->log_name,
                         loop_expression->module_name, loop_expression->source_name,
                         (long) loop_expression->source_line)
                resolved = KAN_FALSE;
            }
        }
        else
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_expression (context, instance, &loop_init_scope,
                                 &((struct kan_rpl_expression_node_t *) expression->children.data)[2u],
                                 &loop_expression->for_.step, &internal_output_type))
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_expression (context, instance, &loop_init_scope,
                                 &((struct kan_rpl_expression_node_t *) expression->children.data)[3u],
                                 &loop_expression->for_.body, &internal_output_type))
        {
            resolved = KAN_FALSE;
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_WHILE:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_WHILE;
        kan_bool_t resolved = KAN_TRUE;
        struct resolve_expression_output_type_t internal_output_type;

        struct resolve_expression_scope_t while_loop_scope = {
            .parent = resolve_scope,
            .function = resolve_scope->function,
            .first_alias = NULL,
            .associated_resolved_scope_if_any = NULL,
            .associated_outer_loop_if_any = new_expression,
        };

        if (resolve_expression (context, instance, &while_loop_scope,
                                &((struct kan_rpl_expression_node_t *) expression->children.data)[0u],
                                &new_expression->while_.condition, &internal_output_type))
        {
            if (!internal_output_type.boolean)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Condition of while cannot be resolved as boolean.", context->log_name,
                         new_expression->module_name, new_expression->source_name, (long) new_expression->source_line)
                resolved = KAN_FALSE;
            }
        }
        else
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_expression (context, instance, &while_loop_scope,
                                 &((struct kan_rpl_expression_node_t *) expression->children.data)[1u],
                                 &new_expression->while_.body, &internal_output_type))
        {
            resolved = KAN_FALSE;
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_BREAK:
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_BREAK;
        new_expression->break_loop = resolve_find_loop_in_current_context (resolve_scope);

        if (!new_expression->break_loop)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Caught break without associated top level loop.", context->log_name,
                     new_expression->module_name, new_expression->source_name, (long) new_expression->source_line)
            return KAN_FALSE;
        }

        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONTINUE:
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_CONTINUE;
        new_expression->continue_loop = resolve_find_loop_in_current_context (resolve_scope);

        if (!new_expression->continue_loop)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s:%s:%s:%ld] Caught continue without associated top level loop.", context->log_name,
                     new_expression->module_name, new_expression->source_name, (long) new_expression->source_line)
            return KAN_FALSE;
        }

        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_RETURN:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_RETURN;
        kan_bool_t resolved = KAN_TRUE;

        if (expression->children.size == 1u)
        {
            struct resolve_expression_output_type_t internal_output_type;
            if (resolve_expression (context, instance, resolve_scope,
                                    &((struct kan_rpl_expression_node_t *) expression->children.data)[0u],
                                    &new_expression->return_expression, &internal_output_type))
            {
                if (!resolve_scope->function->return_type_if_vector &&
                    !resolve_scope->function->return_type_if_matrix && !resolve_scope->function->return_type_if_struct)
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Caught attempt to return \"%s\" from function \"%s\" which returns void.",
                             context->log_name, new_expression->module_name, new_expression->source_name,
                             (long) new_expression->source_line,
                             get_type_name_for_logging (internal_output_type.type_if_vector,
                                                        internal_output_type.type_if_matrix,
                                                        internal_output_type.type_if_struct),
                             resolve_scope->function->name)
                    resolved = KAN_FALSE;
                }

                if (internal_output_type.array_dimensions_count > 0u)
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Caught return of array from function \"%s\" which is not supported.",
                             context->log_name, new_expression->module_name, new_expression->source_name,
                             (long) new_expression->source_line, resolve_scope->function->name)
                    resolved = KAN_FALSE;
                }

                if (internal_output_type.boolean)
                {
                    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                             "[%s:%s:%s:%ld] Caught return of boolean from function \"%s\" which is not supported as "
                             "independent type.",
                             context->log_name, new_expression->module_name, new_expression->source_name,
                             (long) new_expression->source_line, resolve_scope->function->name)
                    resolved = KAN_FALSE;
                }

                if ((resolve_scope->function->return_type_if_vector &&
                     internal_output_type.type_if_vector != resolve_scope->function->return_type_if_vector) ||
                    (resolve_scope->function->return_type_if_matrix &&
                     internal_output_type.type_if_matrix != resolve_scope->function->return_type_if_matrix) ||
                    (resolve_scope->function->return_type_if_struct &&
                     internal_output_type.type_if_struct != resolve_scope->function->return_type_if_struct))
                {
                    KAN_LOG (
                        rpl_compiler_context, KAN_LOG_ERROR,
                        "[%s:%s:%s:%ld] Caught attempt to return \"%s\" from function \"%s\" which returns \"%s\".",
                        context->log_name, new_expression->module_name, new_expression->source_name,
                        (long) new_expression->source_line,
                        get_type_name_for_logging (internal_output_type.type_if_vector,
                                                   internal_output_type.type_if_matrix,
                                                   internal_output_type.type_if_struct),
                        resolve_scope->function->name,
                        get_type_name_for_logging (resolve_scope->function->return_type_if_vector,
                                                   resolve_scope->function->return_type_if_matrix,
                                                   resolve_scope->function->return_type_if_struct))
                    resolved = KAN_FALSE;
                }
            }
            else
            {
                resolved = KAN_FALSE;
            }
        }
        else
        {
            new_expression->return_expression = NULL;
            if (resolve_scope->function->return_type_if_vector || resolve_scope->function->return_type_if_matrix ||
                resolve_scope->function->return_type_if_struct)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s:%s:%ld] Caught void return from function \"%s\" which returns \"%s\".",
                         context->log_name, new_expression->module_name, new_expression->source_name,
                         (long) new_expression->source_line, resolve_scope->function->name,
                         get_type_name_for_logging (resolve_scope->function->return_type_if_vector,
                                                    resolve_scope->function->return_type_if_matrix,
                                                    resolve_scope->function->return_type_if_struct))
                resolved = KAN_FALSE;
            }
        }

        return resolved;
    }
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static kan_bool_t resolve_new_used_function (struct rpl_compiler_context_t *context,
                                             struct rpl_compiler_instance_t *instance,
                                             kan_interned_string_t intermediate_log_name,
                                             struct kan_rpl_function_t *function,
                                             enum kan_rpl_pipeline_stage_t context_stage,
                                             struct compiler_instance_function_node_t **output_node)
{
    if (is_global_name_occupied (instance, function->name))
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s:%s:%ld] Cannot resolve function \"%s\" as its global name is already occupied.",
                 context->log_name, intermediate_log_name, function->source_name, (long) function->source_line,
                 function->name)

        *output_node = NULL;
        return KAN_FALSE;
    }

    struct compiler_instance_function_node_t *function_node = kan_stack_group_allocator_allocate (
        &instance->resolve_allocator, sizeof (struct compiler_instance_function_node_t),
        _Alignof (struct compiler_instance_function_node_t));
    *output_node = function_node;

    kan_bool_t resolved = KAN_TRUE;
    function_node->name = function->name;

    function_node->return_type_if_vector = NULL;
    function_node->return_type_if_matrix = NULL;
    function_node->return_type_if_struct = NULL;

    if (function->return_type_name != interned_void)
    {
        if (!(function_node->return_type_if_vector = find_inbuilt_vector_type (function->return_type_name)) &&
            !(function_node->return_type_if_matrix = find_inbuilt_matrix_type (function->return_type_name)) &&
            !resolve_use_struct (context, instance, function->return_type_name, &function_node->return_type_if_struct))
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Function return type \"%s\" is unknown.",
                     context->log_name, intermediate_log_name, function->source_name, (long) function->source_line,
                     function->return_type_name)
            resolved = KAN_FALSE;
        }
    }

    function_node->has_stage_specific_access = KAN_FALSE;
    function_node->required_stage = context_stage;
    function_node->first_buffer_access = NULL;
    function_node->first_sampler_access = NULL;

    function_node->module_name = intermediate_log_name;
    function_node->source_name = function->source_name;
    function_node->source_line = function->source_line;

    if (!resolve_declarations (context, instance, intermediate_log_name, &function->arguments,
                               &function_node->first_argument, KAN_TRUE))
    {
        resolved = KAN_FALSE;
    }

    struct compiler_instance_declaration_node_t *argument_declaration = function_node->first_argument;
    function_node->first_argument_variable = NULL;
    struct compiler_instance_scope_variable_item_t *last_argument_variable = NULL;

    while (argument_declaration)
    {
        struct compiler_instance_scope_variable_item_t *item = kan_stack_group_allocator_allocate (
            &instance->resolve_allocator, sizeof (struct compiler_instance_scope_variable_item_t),
            _Alignof (struct compiler_instance_scope_variable_item_t));

        item->next = NULL;
        item->variable = &argument_declaration->variable;

        if (last_argument_variable)
        {
            last_argument_variable->next = item;
        }
        else
        {
            function_node->first_argument_variable = item;
        }

        last_argument_variable = item;
        argument_declaration = argument_declaration->next;
    }

    struct resolve_expression_scope_t root_scope = {
        .parent = NULL,
        .function = function_node,
        .first_alias = NULL,
        .associated_resolved_scope_if_any = NULL,
        .associated_outer_loop_if_any = NULL,
    };

    struct resolve_expression_output_type_t output_type_mute;
    if (!resolve_expression (context, instance, &root_scope, &function->body, &function_node->body, &output_type_mute))
    {
        resolved = KAN_FALSE;
    }

    // Parser should not produce conditionals as function bodies anyway, so this should be impossible.
    KAN_ASSERT (function_node->body)

    function_node->next = NULL;
    if (instance->last_function)
    {
        instance->last_function->next = function_node;
    }
    else
    {
        instance->first_function = function_node;
    }

    instance->last_function = function_node;
    return resolved;
}

static inline struct compiler_instance_function_node_t *resolve_inbuilt_function (
    struct compiler_instance_function_node_t *library_first, kan_interned_string_t name)
{
    struct compiler_instance_function_node_t *node = library_first;
    while (node)
    {
        if (node->name == name)
        {
            return node;
        }

        node = node->next;
    }

    return NULL;
}

static inline kan_bool_t resolve_function_check_usability (struct rpl_compiler_context_t *context,
                                                           struct compiler_instance_function_node_t *function_node,
                                                           enum kan_rpl_pipeline_stage_t context_stage)
{
    if (function_node->has_stage_specific_access && function_node->required_stage != context_stage)
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                 "[%s:%s] Function \"%s\" accesses stage specific globals for stage \"%s\", but is also called "
                 "from stage \"%s\", which results in undefined behavior. Dumping list of accessed "
                 "stage-specific globals.",
                 context->log_name, function_node->module_name, function_node->name,
                 get_stage_name (function_node->required_stage), get_stage_name (context_stage))

        struct compiler_instance_buffer_access_node_t *buffer_access = function_node->first_buffer_access;
        while (buffer_access)
        {
            switch (buffer_access->buffer->type)
            {
            case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
            case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
            case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
            case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
                KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                         "[%s:%s] Function \"%s\" accesses stage-specific buffer \"%s\" through call of "
                         "function \"%s\".",
                         context->log_name, function_node->module_name, function_node->name,
                         buffer_access->buffer->name, buffer_access->direct_access_function->name)
                break;

            case KAN_RPL_BUFFER_TYPE_UNIFORM:
            case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
                break;
            }

            buffer_access = buffer_access->next;
        }

        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static kan_bool_t resolve_function_by_name (struct rpl_compiler_context_t *context,
                                            struct rpl_compiler_instance_t *instance,
                                            kan_interned_string_t function_name,
                                            enum kan_rpl_pipeline_stage_t context_stage,
                                            struct compiler_instance_function_node_t **output_node)
{
    // Check inbuilt functions first.
    switch (context->pipeline_type)
    {
    case KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC:
        if ((*output_node = resolve_inbuilt_function (glsl_450_builtin_functions_first, function_name)))
        {
            return resolve_function_check_usability (context, *output_node, context_stage);
        }

        if ((*output_node = resolve_inbuilt_function (shader_standard_builtin_functions_first, function_name)))
        {
            return resolve_function_check_usability (context, *output_node, context_stage);
        }

        break;
    }

    *output_node = NULL;
    struct compiler_instance_function_node_t *function_node = instance->first_function;

    while (function_node)
    {
        if (function_node->name == function_name)
        {
            if (!resolve_function_check_usability (context, function_node, context_stage))
            {
                return KAN_FALSE;
            }

            // Already resolved and no stage conflict.
            *output_node = function_node;
            return KAN_TRUE;
        }

        function_node = function_node->next;
    }

    kan_bool_t result = KAN_TRUE;
    kan_bool_t resolved = KAN_FALSE;

    for (uint64_t intermediate_index = 0u; intermediate_index < context->modules.size; ++intermediate_index)
    {
        struct kan_rpl_intermediate_t *intermediate =
            ((struct kan_rpl_intermediate_t **) context->modules.data)[intermediate_index];

        for (uint64_t function_index = 0u; function_index < intermediate->functions.size; ++function_index)
        {
            struct kan_rpl_function_t *function =
                &((struct kan_rpl_function_t *) intermediate->functions.data)[function_index];

            if (function->name == function_name)
            {
                switch (evaluate_conditional (context, intermediate->log_name, &function->conditional, KAN_TRUE))
                {
                case CONDITIONAL_EVALUATION_RESULT_FAILED:
                    result = KAN_FALSE;
                    break;

                case CONDITIONAL_EVALUATION_RESULT_TRUE:
                    if (resolved)
                    {
                        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                                 "[%s:%s] There are multiple active prototypes of function with name \"%s\".",
                                 context->log_name, intermediate->log_name, function_name)
                        result = KAN_FALSE;
                    }
                    else
                    {
                        if (!resolve_new_used_function (context, instance, intermediate->log_name, function,
                                                        context_stage, &function_node))
                        {
                            result = KAN_FALSE;
                        }

                        resolved = KAN_TRUE;
                    }

                    break;

                case CONDITIONAL_EVALUATION_RESULT_FALSE:
                    break;
                }
            }
        }
    }

    if (!resolved)
    {
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s] Unable to find any active function \"%s\".",
                 context->log_name, function_name)
        return KAN_FALSE;
    }

    *output_node = function_node;
    return result;
}

kan_rpl_compiler_instance_t kan_rpl_compiler_context_resolve (kan_rpl_compiler_context_t compiler_context,
                                                              uint64_t entry_point_count,
                                                              struct kan_rpl_entry_point_t *entry_points)
{
    struct rpl_compiler_context_t *context = (struct rpl_compiler_context_t *) compiler_context;
    struct rpl_compiler_instance_t *instance =
        kan_allocate_general (rpl_compiler_instance_allocation_group, sizeof (struct rpl_compiler_instance_t),
                              _Alignof (struct rpl_compiler_instance_t));

    instance->pipeline_type = context->pipeline_type;
    instance->context_log_name = context->log_name;
    kan_stack_group_allocator_init (&instance->resolve_allocator, rpl_compiler_instance_allocation_group,
                                    KAN_RPL_COMPILER_INSTANCE_RESOLVE_STACK);
    instance->entry_point_count = entry_point_count;

    if (instance->entry_point_count > 0u)
    {
        instance->entry_points = kan_stack_group_allocator_allocate (
            &instance->resolve_allocator, sizeof (struct kan_rpl_entry_point_t) * entry_point_count,
            _Alignof (struct kan_rpl_entry_point_t));
        memcpy (instance->entry_points, entry_points, sizeof (struct kan_rpl_entry_point_t) * entry_point_count);
    }
    else
    {
        instance->entry_points = NULL;
    }

    instance->first_setting = NULL;
    instance->last_setting = NULL;

    instance->first_struct = NULL;
    instance->last_struct = NULL;

    instance->first_buffer = NULL;
    instance->last_buffer = NULL;

    instance->first_sampler = NULL;
    instance->last_sampler = NULL;

    instance->first_function = NULL;
    instance->last_function = NULL;

    kan_bool_t successfully_resolved = KAN_TRUE;
    struct binding_location_assignment_counter_t assignment_counter = {
        .next_attribute_buffer_binding = 0u,
        .next_arbitrary_buffer_binding = 0u,
        .next_attribute_location = 0u,
        .next_vertex_output_location = 0u,
        .next_fragment_output_location = 0u,
    };

    for (uint64_t intermediate_index = 0u; intermediate_index < context->modules.size; ++intermediate_index)
    {
        struct kan_rpl_intermediate_t *intermediate =
            ((struct kan_rpl_intermediate_t **) context->modules.data)[intermediate_index];

        if (!resolve_settings (context, instance, intermediate->log_name, &intermediate->settings,
                               &instance->first_setting, &instance->last_setting))
        {
            successfully_resolved = KAN_FALSE;
        }

        // Buffers and samplers are always added even if they're not used to preserve shader family compatibility.

        if (!resolve_buffers (context, instance, intermediate, &assignment_counter))
        {
            successfully_resolved = KAN_FALSE;
        }

        if (!resolve_samplers (context, instance, intermediate, &assignment_counter))
        {
            successfully_resolved = KAN_FALSE;
        }
    }

    for (uint64_t entry_point_index = 0u; entry_point_index < entry_point_count; ++entry_point_index)
    {
        struct compiler_instance_function_node_t *mute;
        if (!resolve_function_by_name (context, instance, entry_points[entry_point_index].function_name,
                                       entry_points[entry_point_index].stage, &mute))
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,
                     "[%s] Failed to resolve entry point at stage \"%s\" with function \"%s\".", context->log_name,
                     get_stage_name (entry_points[entry_point_index].stage),
                     entry_points[entry_point_index].function_name)
            successfully_resolved = KAN_FALSE;
        }
    }

    kan_stack_group_allocator_reset (&context->resolve_allocator);
    if (successfully_resolved)
    {
        return (kan_rpl_compiler_instance_t) instance;
    }

    kan_rpl_compiler_instance_destroy ((kan_rpl_compiler_instance_t) instance);
    return KAN_INVALID_RPL_COMPILER_INSTANCE;
}

#define SETTING_REQUIRE_TYPE(TYPE, TYPE_NAME)                                                                          \
    if (setting->type != TYPE)                                                                                         \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Setting \"%s\" should have " TYPE_NAME " type.", \
                 instance->context_log_name, setting->module_name, setting->source_name, (long) setting->source_line,  \
                 setting->name)                                                                                        \
        valid = KAN_FALSE;                                                                                             \
    }                                                                                                                  \
    else

#define SETTING_STRING_VALUE(INTERNED_VALUE, REAL_VALUE, OUTPUT)                                                       \
    if (setting->string == INTERNED_VALUE)                                                                             \
    {                                                                                                                  \
        OUTPUT = REAL_VALUE;                                                                                           \
    }                                                                                                                  \
    else

#define SETTING_STRING_NO_MORE_VALUES                                                                                  \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Setting \"%s\" has unknown value \"%s\".",       \
                 instance->context_log_name, setting->module_name, setting->source_name, (long) setting->source_line,  \
                 setting->name, setting->string)                                                                       \
        valid = KAN_FALSE;                                                                                             \
    }

static kan_bool_t emit_meta_graphics_classic_settings (struct rpl_compiler_instance_t *instance,
                                                       struct kan_rpl_meta_t *meta)
{
    kan_bool_t valid = KAN_TRUE;
    meta->graphics_classic_settings = kan_rpl_graphics_classic_pipeline_settings_default ();
    struct compiler_instance_setting_node_t *setting = instance->first_setting;

    while (setting)
    {
        if (setting->name == interned_polygon_mode)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
            {
                SETTING_STRING_VALUE (interned_fill, KAN_RPL_POLYGON_MODE_FILL,
                                      meta->graphics_classic_settings.polygon_mode)
                SETTING_STRING_VALUE (interned_wireframe, KAN_RPL_POLYGON_MODE_WIREFRAME,
                                      meta->graphics_classic_settings.polygon_mode)
                SETTING_STRING_NO_MORE_VALUES
            }
        }
        else if (setting->name == interned_cull_mode)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
            {
                SETTING_STRING_VALUE (interned_back, KAN_RPL_CULL_MODE_BACK, meta->graphics_classic_settings.cull_mode)
                SETTING_STRING_NO_MORE_VALUES
            }
        }
        else if (setting->name == interned_depth_test)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLAG, "flag")
            {
                meta->graphics_classic_settings.depth_test = setting->flag;
            }
        }
        else if (setting->name == interned_depth_write)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLAG, "flag")
            {
                meta->graphics_classic_settings.depth_write = setting->flag;
            }
        }
        else
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Unknown global settings \"%s\".",
                     instance->context_log_name, setting->module_name, setting->source_name,
                     (long) setting->source_line, setting->name)
            valid = KAN_FALSE;
        }

        setting = setting->next;
    }

    return valid;
}

static kan_bool_t emit_meta_sampler_settings (struct rpl_compiler_instance_t *instance,
                                              struct compiler_instance_sampler_node_t *sampler,
                                              struct kan_rpl_meta_sampler_settings_t *settings_output)
{
    kan_bool_t valid = KAN_TRUE;
    *settings_output = kan_rpl_meta_sampler_settings_default ();
    struct compiler_instance_setting_node_t *setting = sampler->first_setting;

    while (setting)
    {
        if (setting->name == interned_mag_filter)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
            {
                SETTING_STRING_VALUE (interned_nearest, KAN_RPL_META_SAMPLER_FILTER_NEAREST,
                                      settings_output->mag_filter)
                SETTING_STRING_VALUE (interned_linear, KAN_RPL_META_SAMPLER_FILTER_LINEAR, settings_output->mag_filter)
                SETTING_STRING_NO_MORE_VALUES
            }
        }
        else if (setting->name == interned_min_filter)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
            {
                SETTING_STRING_VALUE (interned_nearest, KAN_RPL_META_SAMPLER_FILTER_NEAREST,
                                      settings_output->min_filter)
                SETTING_STRING_VALUE (interned_linear, KAN_RPL_META_SAMPLER_FILTER_LINEAR, settings_output->min_filter)
                SETTING_STRING_NO_MORE_VALUES
            }
        }
        else if (setting->name == interned_mip_map_mode)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
            {
                SETTING_STRING_VALUE (interned_nearest, KAN_RPL_META_SAMPLER_MIP_MAP_MODE_NEAREST,
                                      settings_output->mip_map_mode)
                SETTING_STRING_VALUE (interned_linear, KAN_RPL_META_SAMPLER_MIP_MAP_MODE_LINEAR,
                                      settings_output->mip_map_mode)
                SETTING_STRING_NO_MORE_VALUES
            }
        }
        else if (setting->name == interned_address_mode_u)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
            {
                SETTING_STRING_VALUE (interned_repeat, KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT,
                                      settings_output->address_mode_u)
                SETTING_STRING_VALUE (interned_mirrored_repeat, KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
                                      settings_output->address_mode_u)
                SETTING_STRING_VALUE (interned_clamp_to_edge, KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                      settings_output->address_mode_u)
                SETTING_STRING_VALUE (interned_clamp_to_border, KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                      settings_output->address_mode_u)
                SETTING_STRING_VALUE (interned_mirror_clamp_to_edge,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
                                      settings_output->address_mode_u)
                SETTING_STRING_VALUE (interned_mirror_clamp_to_border,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_BORDER,
                                      settings_output->address_mode_u)
                SETTING_STRING_NO_MORE_VALUES
            }
        }
        else if (setting->name == interned_address_mode_v)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
            {
                SETTING_STRING_VALUE (interned_repeat, KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT,
                                      settings_output->address_mode_v)
                SETTING_STRING_VALUE (interned_mirrored_repeat, KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
                                      settings_output->address_mode_v)
                SETTING_STRING_VALUE (interned_clamp_to_edge, KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                      settings_output->address_mode_v)
                SETTING_STRING_VALUE (interned_clamp_to_border, KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                      settings_output->address_mode_v)
                SETTING_STRING_VALUE (interned_mirror_clamp_to_edge,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
                                      settings_output->address_mode_v)
                SETTING_STRING_VALUE (interned_mirror_clamp_to_border,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_BORDER,
                                      settings_output->address_mode_v)
                SETTING_STRING_NO_MORE_VALUES
            }
        }
        else if (setting->name == interned_address_mode_w)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
            {
                SETTING_STRING_VALUE (interned_repeat, KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT,
                                      settings_output->address_mode_w)
                SETTING_STRING_VALUE (interned_mirrored_repeat, KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
                                      settings_output->address_mode_w)
                SETTING_STRING_VALUE (interned_clamp_to_edge, KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                      settings_output->address_mode_w)
                SETTING_STRING_VALUE (interned_clamp_to_border, KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                      settings_output->address_mode_w)
                SETTING_STRING_VALUE (interned_mirror_clamp_to_edge,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
                                      settings_output->address_mode_w)
                SETTING_STRING_VALUE (interned_mirror_clamp_to_border,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_BORDER,
                                      settings_output->address_mode_w)
                SETTING_STRING_NO_MORE_VALUES
            }
        }
        else
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Unknown sampler settings \"%s\".",
                     instance->context_log_name, setting->module_name, setting->source_name,
                     (long) setting->source_line, setting->name)
            valid = KAN_FALSE;
        }

        setting = setting->next;
    }

    return valid;
}

#undef SETTING_REQUIRE_TYPE
#undef SETTING_STRING_VALUE
#undef SETTING_STRING_NO_MORE_VALUES

static inline kan_bool_t emit_meta_variable_type_to_meta_type (struct compiler_instance_variable_t *variable,
                                                               enum kan_rpl_meta_variable_type_t *output,
                                                               kan_interned_string_t context_log_name,
                                                               kan_interned_string_t module_name,
                                                               kan_interned_string_t source_name,
                                                               uint64_t source_line)
{
    if (variable->type_if_vector == &type_f1)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_F1;
    }
    else if (variable->type_if_vector == &type_f2)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_F2;
    }
    else if (variable->type_if_vector == &type_f3)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_F3;
    }
    else if (variable->type_if_vector == &type_f4)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_F4;
    }
    else if (variable->type_if_vector == &type_i1)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_I1;
    }
    else if (variable->type_if_vector == &type_i2)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_I2;
    }
    else if (variable->type_if_vector == &type_i3)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_I3;
    }
    else if (variable->type_if_vector == &type_i4)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_I4;
    }
    else if (variable->type_if_matrix == &type_f3x3)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_F3X3;
    }
    else if (variable->type_if_matrix == &type_f4x4)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_F3X3;
    }
    else
    {
        KAN_LOG (
            rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Unable to find meta type for type \"%s\".",
            context_log_name, module_name, source_name, (long) source_line,
            get_type_name_for_logging (variable->type_if_vector, variable->type_if_matrix, variable->type_if_struct))
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static kan_bool_t emit_meta_gather_parameters_process_field (
    struct rpl_compiler_instance_t *instance,
    uint64_t base_offset,
    struct compiler_instance_declaration_node_t *first_declaration,
    struct kan_rpl_meta_buffer_t *meta_output,
    struct flattening_name_generation_buffer_t *name_generation_buffer);

static kan_bool_t emit_meta_gather_parameters_process_field_list (
    struct rpl_compiler_instance_t *instance,
    uint64_t base_offset,
    struct compiler_instance_declaration_node_t *first_declaration,
    struct kan_rpl_meta_buffer_t *meta_output,
    struct flattening_name_generation_buffer_t *name_generation_buffer)
{
    kan_bool_t valid = KAN_TRUE;
    struct compiler_instance_declaration_node_t *field = first_declaration;

    while (field)
    {
        const uint64_t length = name_generation_buffer->length;
        flattening_name_generation_buffer_append (name_generation_buffer, field->variable.name);

        if (!emit_meta_gather_parameters_process_field (instance, base_offset, field, meta_output,
                                                        name_generation_buffer))
        {
            valid = KAN_FALSE;
        }

        flattening_name_generation_buffer_reset (name_generation_buffer, length);
        field = field->next;
    }

    return valid;
}

static kan_bool_t emit_meta_gather_parameters_process_field (
    struct rpl_compiler_instance_t *instance,
    uint64_t base_offset,
    struct compiler_instance_declaration_node_t *field,
    struct kan_rpl_meta_buffer_t *meta_output,
    struct flattening_name_generation_buffer_t *name_generation_buffer)
{
    if (field->variable.type_if_vector || field->variable.type_if_matrix)
    {
        kan_bool_t valid = KAN_TRUE;
        struct kan_rpl_meta_parameter_t *parameter = kan_dynamic_array_add_last (&meta_output->parameters);

        if (!parameter)
        {
            kan_dynamic_array_set_capacity (&meta_output->parameters, KAN_MAX (1u, meta_output->parameters.size * 2u));
            parameter = kan_dynamic_array_add_last (&meta_output->parameters);
            KAN_ASSERT (parameter)
        }

        kan_rpl_meta_parameter_init (parameter);
        parameter->name = kan_string_intern (name_generation_buffer->buffer);
        parameter->offset = base_offset + field->offset;

        if (!emit_meta_variable_type_to_meta_type (&field->variable, &parameter->type, instance->context_log_name,
                                                   field->module_name, field->source_name, field->source_line))
        {
            valid = KAN_FALSE;
        }

        parameter->total_item_count = 1u;
        for (uint64_t index = 0u; index < field->variable.array_dimensions_count; ++index)
        {
            parameter->total_item_count *= field->variable.array_dimensions[index];
        }

        kan_dynamic_array_set_capacity (&parameter->meta, field->meta_count);
        parameter->meta.size = field->meta_count;

        if (field->meta_count > 0u)
        {
            memcpy (parameter->meta.data, field->meta, sizeof (kan_interned_string_t) * field->meta_count);
        }

        return valid;
    }
    else if (field->variable.type_if_struct)
    {
        return emit_meta_gather_parameters_process_field_list (instance, base_offset + field->offset,
                                                               field->variable.type_if_struct->first_field, meta_output,
                                                               name_generation_buffer);
    }

    return KAN_TRUE;
}

kan_bool_t kan_rpl_compiler_instance_emit_meta (kan_rpl_compiler_instance_t compiler_instance,
                                                struct kan_rpl_meta_t *meta)
{
    struct rpl_compiler_instance_t *instance = (struct rpl_compiler_instance_t *) compiler_instance;
    meta->pipeline_type = instance->pipeline_type;
    kan_bool_t valid = KAN_TRUE;

    switch (instance->pipeline_type)
    {
    case KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC:
        if (!emit_meta_graphics_classic_settings (instance, meta))
        {
            valid = KAN_FALSE;
        }

        break;
    }

    uint64_t buffer_count = 0u;
    struct compiler_instance_buffer_node_t *buffer = instance->first_buffer;

    while (buffer)
    {
        ++buffer_count;
        buffer = buffer->next;
    }

    kan_dynamic_array_set_capacity (&meta->buffers, buffer_count);
    buffer = instance->first_buffer;

    while (buffer)
    {
        if (buffer->type == KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT)
        {
            // Not exposed.
            buffer = buffer->next;
            continue;
        }

        if (buffer->type == KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT)
        {
            // Not exposed, only affects pipeline settings.
            if (instance->pipeline_type == KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC)
            {
                struct compiler_instance_buffer_flattened_declaration_t *declaration =
                    buffer->first_flattened_declaration;

                while (declaration)
                {
                    ++meta->graphics_classic_settings.fragment_output_count;
                    declaration = declaration->next;
                }
            }

            buffer = buffer->next;
            continue;
        }

        struct kan_rpl_meta_buffer_t *meta_buffer = kan_dynamic_array_add_last (&meta->buffers);
        KAN_ASSERT (meta_buffer)
        kan_rpl_meta_buffer_init (meta_buffer);

        meta_buffer->name = buffer->name;
        meta_buffer->binding = buffer->binding;
        meta_buffer->type = buffer->type;
        meta_buffer->size = buffer->size;

        if (buffer->type == KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE ||
            buffer->type == KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE)
        {
            uint64_t count = 0u;
            struct compiler_instance_buffer_flattened_declaration_t *flattened_declaration =
                buffer->first_flattened_declaration;

            while (flattened_declaration)
            {
                ++count;
                flattened_declaration = flattened_declaration->next;
            }

            kan_dynamic_array_set_capacity (&meta_buffer->attributes, count);
            flattened_declaration = buffer->first_flattened_declaration;

            while (flattened_declaration)
            {
                struct kan_rpl_meta_attribute_t *meta_attribute = kan_dynamic_array_add_last (&meta_buffer->attributes);
                meta_attribute->location = flattened_declaration->location;
                meta_attribute->offset = flattened_declaration->source_declaration->offset;

                if (!emit_meta_variable_type_to_meta_type (&flattened_declaration->source_declaration->variable,
                                                           &meta_attribute->type, instance->context_log_name,
                                                           flattened_declaration->source_declaration->module_name,
                                                           flattened_declaration->source_declaration->source_name,
                                                           flattened_declaration->source_declaration->source_line))
                {
                    valid = KAN_FALSE;
                }

                flattened_declaration = flattened_declaration->next;
            }
        }

        if (buffer->type == KAN_RPL_BUFFER_TYPE_UNIFORM || buffer->type == KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE ||
            buffer->type == KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE ||
            buffer->type == KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM ||
            buffer->type == KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE)
        {
            struct flattening_name_generation_buffer_t name_generation_buffer;
            const uint64_t buffer_name_length = strlen (buffer->name);
            const uint64_t to_copy = KAN_MIN (KAN_RPL_COMPILER_INSTANCE_MAX_FLAT_NAME_LENGTH - 1u, buffer_name_length);

            name_generation_buffer.length = to_copy;
            name_generation_buffer.buffer[to_copy] = '\0';
            memcpy (name_generation_buffer.buffer, buffer->name, to_copy);

            if (!emit_meta_gather_parameters_process_field_list (instance, 0u, buffer->first_field, meta_buffer,
                                                                 &name_generation_buffer))
            {
                valid = KAN_FALSE;
            }

            kan_dynamic_array_set_capacity (&meta_buffer->parameters, meta_buffer->parameters.size);
        }

        buffer = buffer->next;
    }

    uint64_t sampler_count = 0u;
    struct compiler_instance_sampler_node_t *sampler = instance->first_sampler;

    while (sampler)
    {
        ++sampler_count;
        sampler = sampler->next;
    }

    kan_dynamic_array_set_capacity (&meta->samplers, sampler_count);
    sampler = instance->first_sampler;

    while (sampler)
    {
        struct kan_rpl_meta_sampler_t *meta_sampler = kan_dynamic_array_add_last (&meta->samplers);
        KAN_ASSERT (meta_sampler)

        meta_sampler->name = sampler->name;
        meta_sampler->binding = sampler->binding;
        meta_sampler->type = sampler->type;

        if (!emit_meta_sampler_settings (instance, sampler, &meta_sampler->settings))
        {
            valid = KAN_FALSE;
        }

        sampler = sampler->next;
    }

    return valid;
}

void kan_rpl_compiler_instance_destroy (kan_rpl_compiler_instance_t compiler_instance)
{
    struct rpl_compiler_instance_t *instance = (struct rpl_compiler_instance_t *) compiler_instance;
    kan_stack_group_allocator_shutdown (&instance->resolve_allocator);
    kan_free_general (rpl_compiler_instance_allocation_group, instance, sizeof (struct rpl_compiler_instance_t));
}

void kan_rpl_compiler_context_destroy (kan_rpl_compiler_context_t compiler_context)
{
    struct rpl_compiler_context_t *instance = (struct rpl_compiler_context_t *) compiler_context;
    kan_dynamic_array_shutdown (&instance->option_values);
    kan_dynamic_array_shutdown (&instance->modules);
    kan_stack_group_allocator_shutdown (&instance->resolve_allocator);
    kan_free_general (rpl_compiler_context_allocation_group, instance, sizeof (struct rpl_compiler_context_t));
}
