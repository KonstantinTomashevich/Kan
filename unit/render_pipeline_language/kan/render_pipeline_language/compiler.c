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

struct compiler_instance_buffer_flattened_declaration_t
{
    struct compiler_instance_buffer_flattened_declaration_t *next;
    struct compiler_instance_declaration_node_t *source_declaration;
    kan_interned_string_t readable_name;
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
    struct compiler_instance_declaration_node_t *first_field;

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
    struct compiler_instance_setting_node_t *first_setting;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    uint32_t source_line;
};

enum compiler_instance_expression_type_t
{
    // TODO: Get rid of identifier, binary operation and unary operation.
    //       Replace with identifiers with direct accesses and access chains.
    //       Replace with concrete operations instead of nested binary operation enumeration from intermediate.
    //       Add variable list to scope.
    //       Resolve functions, samplers and constructors in calls.

    COMPILER_INSTANCE_EXPRESSION_TYPE_IDENTIFIER = 0u,
    COMPILER_INSTANCE_EXPRESSION_TYPE_INTEGER_LITERAL,
    COMPILER_INSTANCE_EXPRESSION_TYPE_FLOATING_LITERAL,
    COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_DECLARATION,
    COMPILER_INSTANCE_EXPRESSION_TYPE_BINARY_OPERATION,
    COMPILER_INSTANCE_EXPRESSION_TYPE_UNARY_OPERATION,
    COMPILER_INSTANCE_EXPRESSION_TYPE_SCOPE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_FUNCTION_CALL,
    COMPILER_INSTANCE_EXPRESSION_TYPE_CONSTRUCTOR,
    COMPILER_INSTANCE_EXPRESSION_TYPE_IF,
    COMPILER_INSTANCE_EXPRESSION_TYPE_FOR,
    COMPILER_INSTANCE_EXPRESSION_TYPE_WHILE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_BREAK,
    COMPILER_INSTANCE_EXPRESSION_TYPE_CONTINUE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_RETURN,
};

struct compiler_instance_binary_operation_suffix_t
{
    enum kan_rpl_binary_operation_t operation;
    struct compiler_instance_expression_node_t *left_operand;
    struct compiler_instance_expression_node_t *right_operand;
};

struct compiler_instance_unary_operation_suffix_t
{
    enum kan_rpl_unary_operation_t operation;
    struct compiler_instance_expression_node_t *operand;
};

struct compiler_instance_expression_list_item_t
{
    struct compiler_instance_expression_list_item_t *next;
    struct compiler_instance_expression_node_t *expression;
};

struct compiler_instance_function_call_suffix_t
{
    kan_interned_string_t function_name;
    struct compiler_instance_expression_list_item_t *first_argument;
};

struct compiler_instance_constructor_suffix_t
{
    kan_interned_string_t type_name;
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
        kan_interned_string_t identifier;
        int64_t integer_literal;
        double floating_literal;
        struct compiler_instance_variable_t variable;
        struct compiler_instance_binary_operation_suffix_t binary_operation;
        struct compiler_instance_unary_operation_suffix_t unary_operation;
        struct compiler_instance_expression_list_item_t *scope_first_expression;
        struct compiler_instance_function_call_suffix_t function_call;
        struct compiler_instance_constructor_suffix_t constructor;
        struct compiler_instance_if_suffix_t if_;
        struct compiler_instance_for_suffix_t for_;
        struct compiler_instance_while_suffix_t while_;
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
    kan_interned_string_t return_type;

    struct compiler_instance_declaration_node_t *first_argument;
    struct compiler_instance_expression_node_t *body;

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

struct resolve_expression_alias_node_t
{
    struct resolve_expression_alias_node_t *next;
    kan_interned_string_t name;

    // Compiler instance expressions do not have links to parents, therefore we can safely resolve alias once and
    // paste it as a link to every detected usage.
    struct compiler_instance_expression_node_t *resolved_expression;
};

struct resolve_expression_scope_t
{
    struct resolve_expression_scope_t *parent;
    struct compiler_instance_function_node_t *function;
    struct resolve_expression_alias_node_t *first_alias;
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

// static char inbuilt_element_identifiers[INBUILT_ELEMENT_IDENTIFIERS_ITEMS][INBUILT_ELEMENT_IDENTIFIERS_VARIANTS] = {
//     {'x', 'r'}, {'y', 'g'}, {'z', 'b'}, {'w', 'a'}};

static struct inbuilt_vector_type_t type_f1;
static struct inbuilt_vector_type_t type_f2;
static struct inbuilt_vector_type_t type_f3;
static struct inbuilt_vector_type_t type_f4;
static struct inbuilt_vector_type_t type_i1;
static struct inbuilt_vector_type_t type_i2;
static struct inbuilt_vector_type_t type_i3;
static struct inbuilt_vector_type_t type_i4;
static struct inbuilt_vector_type_t *vector_types[] = {&type_f1, &type_f2, &type_f3, &type_f4,
                                                       &type_i1, &type_i2, &type_i3, &type_i4};
// static struct inbuilt_vector_type_t *floating_vector_types[] = {&type_f1, &type_f2, &type_f3, &type_f4};
// static struct inbuilt_vector_type_t *integer_vector_types[] = {&type_i1, &type_i2, &type_i3, &type_i4};

static struct inbuilt_matrix_type_t type_f3x3;
static struct inbuilt_matrix_type_t type_f4x4;
static struct inbuilt_matrix_type_t *matrix_types[] = {&type_f3x3, &type_f4x4};

static struct compiler_instance_function_node_t *glsl_450_builtin_functions_first;
static struct compiler_instance_function_node_t glsl_450_sqrt;
static struct compiler_instance_declaration_node_t glsl_450_sqrt_arguments[1u];

static struct compiler_instance_function_node_t *shader_standard_builtin_functions_first;
static struct compiler_instance_function_node_t shader_standard_vertex_stage_output_position;
static struct compiler_instance_declaration_node_t shader_standard_vertex_stage_output_position_arguments[1u];

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
            .return_type = kan_string_intern ("f1"),
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
            .next = NULL,
            .name = kan_string_intern ("vertex_stage_output_position"),
            .return_type = kan_string_intern ("void"),
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
                                                struct flattening_name_generation_buffer_t *name_generation_buffer)
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
        // Process struct node.
        struct compiler_instance_buffer_flattening_graph_node_t *last_root = NULL;
        struct compiler_instance_declaration_node_t *field = declaration->variable.type_if_struct->first_field;

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

            if (!flatten_buffer_process_field (context, instance, buffer, field, new_root, name_generation_buffer))
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
                output_node->first_child = new_root;
            }

            last_root = new_root;
            field = field->next;
        }
    }

    return result;
}

static kan_bool_t flatten_buffer (struct rpl_compiler_context_t *context,
                                  struct rpl_compiler_instance_t *instance,
                                  struct compiler_instance_buffer_node_t *buffer)
{
    kan_bool_t result = KAN_TRUE;
    struct flattening_name_generation_buffer_t name_generation_buffer;
    const uint64_t buffer_name_length = strlen (buffer->name);
    const uint64_t to_copy = KAN_MIN (KAN_RPL_COMPILER_INSTANCE_MAX_FLAT_NAME_LENGTH - 1u, buffer_name_length);

    flattening_name_generation_buffer_reset (&name_generation_buffer, to_copy);
    memcpy (name_generation_buffer.buffer, buffer->name, to_copy);

    struct compiler_instance_buffer_flattening_graph_node_t *last_root = NULL;
    struct compiler_instance_declaration_node_t *field = buffer->first_field;

    while (field)
    {
        struct compiler_instance_buffer_flattening_graph_node_t *new_root = kan_stack_group_allocator_allocate (
            &instance->resolve_allocator, sizeof (struct compiler_instance_buffer_flattening_graph_node_t),
            _Alignof (struct compiler_instance_buffer_flattening_graph_node_t));

        new_root->next_on_level = NULL;
        new_root->first_child = NULL;
        new_root->name = field->variable.name;
        new_root->flattened_result = NULL;

        const uint64_t length = name_generation_buffer.length;
        flattening_name_generation_buffer_append (&name_generation_buffer, field->variable.name);

        if (!flatten_buffer_process_field (context, instance, buffer, field, new_root, &name_generation_buffer))
        {
            result = KAN_FALSE;
        }

        flattening_name_generation_buffer_reset (&name_generation_buffer, length);
        if (last_root)
        {
            last_root->next_on_level = new_root;
        }
        else
        {
            buffer->flattening_graph_base = new_root;
        }

        last_root = new_root;
        field = field->next;
    }

    return result;
}

static kan_bool_t resolve_buffers (struct rpl_compiler_context_t *context,
                                   struct rpl_compiler_instance_t *instance,
                                   struct kan_rpl_intermediate_t *intermediate)
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

            target_buffer->flattening_graph_base = NULL;
            target_buffer->first_flattened_declaration = NULL;
            target_buffer->last_flattened_declaration = NULL;

            if (result)
            {
                switch (target_buffer->type)
                {
                case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
                case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
                    flatten_buffer (context, instance, target_buffer);
                    // TODO: Validate no arrays.
                    break;

                case KAN_RPL_BUFFER_TYPE_UNIFORM:
                case KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM:
                    // TODO: Validate for 16-byty alignment compatibility.
                    break;

                case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
                case KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE:
                    break;

                case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
                    flatten_buffer (context, instance, target_buffer);
                    break;

                case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
                    flatten_buffer (context, instance, target_buffer);
                    // TODO: Validate only f4's.
                    break;
                }
            }

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
                                    struct kan_rpl_intermediate_t *intermediate)
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
            struct compiler_instance_sampler_node_t *target_sampler = kan_stack_group_allocator_allocate (
                &instance->resolve_allocator, sizeof (struct compiler_instance_sampler_node_t),
                _Alignof (struct compiler_instance_sampler_node_t));

            target_sampler->next = NULL;
            target_sampler->name = source_sampler->name;
            target_sampler->type = source_sampler->type;
            target_sampler->used = KAN_FALSE;

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

static kan_bool_t check_alias_name_is_not_occupied (struct resolve_expression_scope_t *resolve_scope,
                                                    kan_interned_string_t name)
{
    // TODO: Might need rewrite with addition of variables to scopes.
    struct resolve_expression_alias_node_t *alias_node = resolve_scope->first_alias;
    while (alias_node)
    {
        if (alias_node->name == name)
        {
            return KAN_FALSE;
        }

        alias_node = alias_node->next;
    }

    if (resolve_scope->parent)
    {
        return check_alias_name_is_not_occupied (resolve_scope->parent, name);
    }

    return KAN_TRUE;
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

static kan_bool_t resolve_function_global_access (struct rpl_compiler_context_t *context,
                                                  struct rpl_compiler_instance_t *instance,
                                                  struct compiler_instance_function_node_t *function,
                                                  uint64_t usage_line,
                                                  enum kan_rpl_pipeline_stage_t stage,
                                                  kan_interned_string_t identifier)
{
    // TODO: Might need rewrite with refactor to explicit accesses and access chains.
    struct compiler_instance_buffer_node_t *buffer = instance->first_buffer;
    while (buffer)
    {
        if (buffer->name == identifier)
        {
            return resolve_use_buffer (context, instance, function, usage_line, stage, buffer);
        }

        buffer = buffer->next;
    }

    struct compiler_instance_sampler_node_t *sampler = instance->first_sampler;
    while (sampler)
    {
        if (sampler->name == identifier)
        {
            return resolve_use_sampler (instance, function, sampler);
        }

        sampler = sampler->next;
    }

    // Nothing found, might be local variable, we do not validate it here.
    return KAN_TRUE;
}

static kan_bool_t resolve_expression (struct rpl_compiler_context_t *context,
                                      struct rpl_compiler_instance_t *instance,
                                      struct resolve_expression_scope_t *resolve_scope,
                                      struct kan_rpl_expression_node_t *expression,
                                      struct compiler_instance_expression_node_t **output,
                                      kan_bool_t resolve_identifier_as_access);

static kan_bool_t resolve_expression_array (struct rpl_compiler_context_t *context,
                                            struct rpl_compiler_instance_t *instance,
                                            struct resolve_expression_scope_t *resolve_scope,
                                            struct kan_dynamic_array_t *array,
                                            struct compiler_instance_expression_list_item_t **output)
{
    kan_bool_t resolved = KAN_TRUE;
    struct compiler_instance_expression_list_item_t *last_expression = NULL;

    for (uint64_t index = 0u; index < array->size; ++index)
    {
        struct compiler_instance_expression_node_t *resolved_expression;
        if (resolve_expression (context, instance, resolve_scope,
                                &((struct kan_rpl_expression_node_t *) array->data)[index], &resolved_expression,
                                KAN_TRUE))
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
                *output = list_item;
            }

            last_expression = list_item;
        }
        else
        {
            resolved = KAN_FALSE;
        }
    }

    return resolved;
}

static kan_bool_t resolve_function_by_name (struct rpl_compiler_context_t *context,
                                            struct rpl_compiler_instance_t *instance,
                                            kan_interned_string_t function_name,
                                            enum kan_rpl_pipeline_stage_t context_stage,
                                            struct compiler_instance_function_node_t **output_node);

static kan_bool_t resolve_expression (struct rpl_compiler_context_t *context,
                                      struct rpl_compiler_instance_t *instance,
                                      struct resolve_expression_scope_t *resolve_scope,
                                      struct kan_rpl_expression_node_t *expression,
                                      struct compiler_instance_expression_node_t **output,
                                      kan_bool_t resolve_identifier_as_access)
{
    *output = NULL;

    if (expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_NOPE)
    {
        return KAN_TRUE;
    }
    // We check conditional expressions before anything else as they have special allocation strategy.
    else if (expression->type == KAN_RPL_EXPRESSION_NODE_TYPE_CONDITIONAL_SCOPE)
    {
        switch (evaluate_conditional (context, resolve_scope->function->module_name,
                                      &((struct kan_rpl_expression_node_t *) expression->children.data)[0u], KAN_TRUE))
        {
        case CONDITIONAL_EVALUATION_RESULT_FAILED:
            return KAN_FALSE;

        case CONDITIONAL_EVALUATION_RESULT_TRUE:
            return resolve_expression (context, instance, resolve_scope,
                                       &((struct kan_rpl_expression_node_t *) expression->children.data)[1u], output,
                                       KAN_TRUE);

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
            if (!check_alias_name_is_not_occupied (resolve_scope, expression->alias_name))
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
                                     &alias_node->resolved_expression, KAN_TRUE))
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
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_IDENTIFIER;
        new_expression->identifier = expression->identifier;

        if (resolve_identifier_as_access)
        {
            if (!resolve_function_global_access (context, instance, resolve_scope->function, expression->source_line,
                                                 resolve_scope->function->required_stage, expression->identifier))
            {
                return KAN_FALSE;
            }
        }

        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_INTEGER_LITERAL:
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_INTEGER_LITERAL;
        new_expression->integer_literal = expression->integer_literal;
        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_FLOATING_LITERAL:
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_FLOATING_LITERAL;
        new_expression->floating_literal = expression->floating_literal;
        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_VARIABLE_DECLARATION:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_DECLARATION;
        new_expression->variable.name = expression->variable_declaration.variable_name;
        new_expression->variable.array_dimensions_count = expression->children.size;

        kan_bool_t resolved = KAN_TRUE;
        if (!resolve_variable_type (context, instance, new_expression->module_name, &new_expression->variable,
                                    expression->variable_declaration.type_name,
                                    expression->variable_declaration.variable_name, expression->source_name,
                                    expression->source_line))
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_array_dimensions (context, instance, new_expression->module_name, &new_expression->variable,
                                       &expression->children, KAN_TRUE))
        {
            resolved = KAN_FALSE;
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_BINARY_OPERATION:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_BINARY_OPERATION;
        new_expression->binary_operation.operation = expression->binary_operation;
        kan_bool_t resolved = KAN_TRUE;

        if (!resolve_expression (context, instance, resolve_scope,
                                 &((struct kan_rpl_expression_node_t *) expression->children.data)[0u],
                                 &new_expression->binary_operation.left_operand, KAN_TRUE))
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_expression (context, instance, resolve_scope,
                                 &((struct kan_rpl_expression_node_t *) expression->children.data)[1u],
                                 &new_expression->binary_operation.right_operand,
                                 expression->binary_operation != KAN_RPL_BINARY_OPERATION_FIELD_ACCESS))
        {
            resolved = KAN_FALSE;
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_UNARY_OPERATION:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_UNARY_OPERATION;
        new_expression->unary_operation.operation = expression->unary_operation;
        kan_bool_t resolved = KAN_TRUE;

        if (!resolve_expression (context, instance, resolve_scope,
                                 &((struct kan_rpl_expression_node_t *) expression->children.data)[0u],
                                 &new_expression->unary_operation.operand, KAN_TRUE))
        {
            resolved = KAN_FALSE;
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_SCOPE:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_SCOPE;
        new_expression->scope_first_expression = NULL;

        struct resolve_expression_scope_t child_scope = {
            .parent = resolve_scope,
            .function = resolve_scope->function,
            .first_alias = NULL,
        };

        return resolve_expression_array (context, instance, &child_scope, &expression->children,
                                         &new_expression->scope_first_expression);
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_FUNCTION_CALL:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_FUNCTION_CALL;
        new_expression->function_call.function_name = expression->function_name;
        kan_bool_t resolved = KAN_TRUE;

        if (!resolve_expression_array (context, instance, resolve_scope, &expression->children,
                                       &new_expression->function_call.first_argument))
        {
            resolved = KAN_FALSE;
        }

        struct compiler_instance_sampler_node_t *sampler = instance->first_sampler;
        while (sampler)
        {
            if (sampler->name == expression->function_name)
            {
                if (!resolve_use_sampler (instance, resolve_scope->function, sampler))
                {
                    resolved = KAN_FALSE;
                }

                break;
            }

            sampler = sampler->next;
        }

        if (!sampler)
        {
            struct compiler_instance_function_node_t *temporary_mute;
            if (!resolve_function_by_name (context, instance, expression->function_name,
                                           resolve_scope->function->required_stage, &temporary_mute))
            {
                resolved = KAN_FALSE;
            }
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONSTRUCTOR:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_CONSTRUCTOR;
        new_expression->constructor.type_name = expression->constructor_type_name;
        kan_bool_t resolved = KAN_TRUE;

        if (!resolve_expression_array (context, instance, resolve_scope, &expression->children,
                                       &new_expression->constructor.first_argument))
        {
            resolved = KAN_FALSE;
        }

        if (!find_inbuilt_vector_type (expression->constructor_type_name) &&
            !find_inbuilt_matrix_type (expression->constructor_type_name))
        {
            struct compiler_instance_struct_node_t *temporary_mute;
            if (!resolve_use_struct (context, instance, expression->constructor_type_name, &temporary_mute))
            {
                resolved = KAN_FALSE;
            }
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_IF:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_IF;
        kan_bool_t resolved = KAN_TRUE;

        if (!resolve_expression (context, instance, resolve_scope,
                                 &((struct kan_rpl_expression_node_t *) expression->children.data)[0u],
                                 &new_expression->if_.condition, KAN_TRUE))
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_expression (context, instance, resolve_scope,
                                 &((struct kan_rpl_expression_node_t *) expression->children.data)[1u],
                                 &new_expression->if_.when_true, KAN_TRUE))
        {
            resolved = KAN_FALSE;
        }

        if (expression->children.size == 3u)
        {
            if (!resolve_expression (context, instance, resolve_scope,
                                     &((struct kan_rpl_expression_node_t *) expression->children.data)[2u],
                                     &new_expression->if_.when_false, KAN_TRUE))
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
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_FOR;
        kan_bool_t resolved = KAN_TRUE;

        struct resolve_expression_scope_t loop_init_scope = {
            .parent = resolve_scope,
            .function = resolve_scope->function,
            .first_alias = NULL,
        };

        if (!resolve_expression (context, instance, &loop_init_scope,
                                 &((struct kan_rpl_expression_node_t *) expression->children.data)[0u],
                                 &new_expression->for_.init, KAN_TRUE))
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_expression (context, instance, &loop_init_scope,
                                 &((struct kan_rpl_expression_node_t *) expression->children.data)[1u],
                                 &new_expression->for_.condition, KAN_TRUE))
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_expression (context, instance, &loop_init_scope,
                                 &((struct kan_rpl_expression_node_t *) expression->children.data)[2u],
                                 &new_expression->for_.step, KAN_TRUE))
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_expression (context, instance, &loop_init_scope,
                                 &((struct kan_rpl_expression_node_t *) expression->children.data)[3u],
                                 &new_expression->for_.body, KAN_TRUE))
        {
            resolved = KAN_FALSE;
        }
        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_WHILE:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_WHILE;
        kan_bool_t resolved = KAN_TRUE;

        if (!resolve_expression (context, instance, resolve_scope,
                                 &((struct kan_rpl_expression_node_t *) expression->children.data)[0u],
                                 &new_expression->while_.condition, KAN_TRUE))
        {
            resolved = KAN_FALSE;
        }

        if (!resolve_expression (context, instance, resolve_scope,
                                 &((struct kan_rpl_expression_node_t *) expression->children.data)[1u],
                                 &new_expression->while_.body, KAN_TRUE))
        {
            resolved = KAN_FALSE;
        }

        return resolved;
    }

    case KAN_RPL_EXPRESSION_NODE_TYPE_BREAK:
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_BREAK;
        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_CONTINUE:
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_CONTINUE;
        return KAN_TRUE;

    case KAN_RPL_EXPRESSION_NODE_TYPE_RETURN:
    {
        new_expression->type = COMPILER_INSTANCE_EXPRESSION_TYPE_RETURN;
        kan_bool_t resolved = KAN_TRUE;

        if (expression->children.size == 1u)
        {
            if (!resolve_expression (context, instance, resolve_scope,
                                     &((struct kan_rpl_expression_node_t *) expression->children.data)[0u],
                                     &new_expression->return_expression, KAN_TRUE))
            {
                resolved = KAN_FALSE;
            }
        }
        else
        {
            new_expression->return_expression = NULL;
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
    struct compiler_instance_function_node_t *function_node = kan_stack_group_allocator_allocate (
        &instance->resolve_allocator, sizeof (struct compiler_instance_function_node_t),
        _Alignof (struct compiler_instance_function_node_t));
    *output_node = function_node;

    function_node->name = function->name;
    function_node->return_type = function->return_type_name;

    function_node->has_stage_specific_access = KAN_FALSE;
    function_node->required_stage = context_stage;
    function_node->first_buffer_access = NULL;
    function_node->first_sampler_access = NULL;

    function_node->module_name = intermediate_log_name;
    function_node->source_name = function->source_name;
    function_node->source_line = function->source_line;

    kan_bool_t resolved = KAN_TRUE;
    if (!resolve_declarations (context, instance, intermediate_log_name, &function->arguments,
                               &function_node->first_argument, KAN_TRUE))
    {
        resolved = KAN_FALSE;
    }

    struct resolve_expression_scope_t root_scope = {
        .parent = NULL,
        .function = function_node,
        .first_alias = NULL,
    };

    if (!resolve_expression (context, instance, &root_scope, &function->body, &function_node->body, KAN_TRUE))
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

        if (!resolve_buffers (context, instance, intermediate))
        {
            successfully_resolved = KAN_FALSE;
        }

        if (!resolve_samplers (context, instance, intermediate))
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
