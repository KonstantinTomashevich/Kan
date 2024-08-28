// It was decided to split compiler implementation to several files, because modern IDEs like CLion start to glitch
// a lot on files over 8000 lines of code. And compiler implementation has potential to grow up to 20000 lines of code
// or perhaps even more, because support for multiple backends might require lots of code.
#if !defined(KAN_RPL_COMPILER_IMPLEMENTATION)
#    error                                                                                                             \
        "kan/render_pipeline_language/compiler_internal.h should only be included by implementation as it has no stage API and is subject to lots of changes during development."
#endif

#include <stddef.h>
#include <string.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/container/hash_storage.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/render_pipeline_language/compiler.h>
#include <kan/threading/atomic.h>

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
    struct kan_rpl_expression_t conditional;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    uint32_t source_line;
};

struct compiler_instance_full_type_definition_t
{
    struct inbuilt_vector_type_t *if_vector;
    struct inbuilt_matrix_type_t *if_matrix;
    struct compiler_instance_struct_node_t *if_struct;

    uint64_t array_dimensions_count;
    uint64_t *array_dimensions;
};

struct compiler_instance_variable_t
{
    kan_interned_string_t name;
    struct compiler_instance_full_type_definition_t type;
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

    uint32_t spirv_id_value;
    uint32_t spirv_id_function_pointer;
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

    uint32_t spirv_id_input;
    uint32_t spirv_id_output;
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

    uint32_t structured_variable_spirv_id;

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

    uint32_t variable_spirv_id;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    uint32_t source_line;
};

enum compiler_instance_expression_type_t
{
    COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_REFERENCE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS,
    COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS_INPUT,
    COMPILER_INSTANCE_EXPRESSION_TYPE_FLATTENED_BUFFER_ACCESS_OUTPUT,
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

struct compiler_instance_variable_declaration_suffix_t
{
    struct compiler_instance_variable_t variable;
    struct compiler_instance_scope_variable_item_t *declared_in_scope;
};

struct compiler_instance_binary_operation_suffix_t
{
    struct compiler_instance_expression_node_t *left_operand;
    struct compiler_instance_expression_node_t *right_operand;
};

struct compiler_instance_unary_operation_suffix_t
{
    struct compiler_instance_expression_node_t *operand;
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
    kan_bool_t writable;
    uint32_t spirv_id;
};

struct compiler_instance_scope_suffix_t
{
    struct compiler_instance_scope_variable_item_t *first_variable;
    struct compiler_instance_expression_list_item_t *first_expression;
    kan_bool_t leads_to_return;
    kan_bool_t leads_to_jump;
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

    uint32_t spirv_label_break;
    uint32_t spirv_label_continue;
};

struct compiler_instance_while_suffix_t
{
    struct compiler_instance_expression_node_t *condition;
    struct compiler_instance_expression_node_t *body;

    uint32_t spirv_label_break;
    uint32_t spirv_label_continue;
};

struct compiler_instance_expression_output_type_t
{
    struct compiler_instance_full_type_definition_t type;
    kan_bool_t boolean;
    kan_bool_t writable;
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
        struct compiler_instance_variable_declaration_suffix_t variable_declaration;
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

    struct compiler_instance_expression_output_type_t output;
    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    uint32_t source_line;
};

struct compiler_instance_buffer_access_node_t
{
    struct compiler_instance_buffer_access_node_t *next;
    struct compiler_instance_buffer_node_t *buffer;
    struct compiler_instance_function_node_t *direct_access_function;
    kan_bool_t used_as_output;
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

    uint32_t spirv_id;
    uint32_t spirv_external_library_id;
    uint32_t spirv_external_instruction_id;
    const struct spirv_generation_function_type_t *spirv_function_type;

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
    uint32_t spirv_id_input_pointer;
    uint32_t spirv_id_output_pointer;
    uint32_t spirv_id_function_pointer;
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
    uint32_t spirv_id_input_pointer;
    uint32_t spirv_id_output_pointer;
    uint32_t spirv_id_function_pointer;
};

/// \details In common header as they are needed to initialize statics with known fixed ids.
enum spirv_fixed_ids_t
{
    SPIRV_FIXED_ID_INVALID = 0u,

    SPIRV_FIXED_ID_TYPE_VOID = 1u,
    SPIRV_FIXED_ID_TYPE_BOOLEAN,

    SPIRV_FIXED_ID_TYPE_FLOAT,
    SPIRV_FIXED_ID_TYPE_FLOAT_INPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_FLOAT_OUTPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_FLOAT_FUNCTION_POINTER,

    SPIRV_FIXED_ID_TYPE_INTEGER,
    SPIRV_FIXED_ID_TYPE_INTEGER_INPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_INTEGER_OUTPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_INTEGER_FUNCTION_POINTER,

    SPIRV_FIXED_ID_TYPE_F2,
    SPIRV_FIXED_ID_TYPE_F2_INPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_F2_OUTPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_F2_FUNCTION_POINTER,

    SPIRV_FIXED_ID_TYPE_F3,
    SPIRV_FIXED_ID_TYPE_F3_INPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_F3_OUTPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_F3_FUNCTION_POINTER,

    SPIRV_FIXED_ID_TYPE_F4,
    SPIRV_FIXED_ID_TYPE_F4_INPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_F4_OUTPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_F4_FUNCTION_POINTER,

    SPIRV_FIXED_ID_TYPE_I2,
    SPIRV_FIXED_ID_TYPE_I2_INPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_I2_OUTPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_I2_FUNCTION_POINTER,

    SPIRV_FIXED_ID_TYPE_I3,
    SPIRV_FIXED_ID_TYPE_I3_INPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_I3_OUTPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_I3_FUNCTION_POINTER,

    SPIRV_FIXED_ID_TYPE_I4,
    SPIRV_FIXED_ID_TYPE_I4_INPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_I4_OUTPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_I4_FUNCTION_POINTER,

    SPIRV_FIXED_ID_TYPE_F3X3,
    SPIRV_FIXED_ID_TYPE_F3X3_INPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_F3X3_OUTPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_F3X3_FUNCTION_POINTER,

    SPIRV_FIXED_ID_TYPE_F4X4,
    SPIRV_FIXED_ID_TYPE_F4X4_INPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_F4X4_OUTPUT_POINTER,
    SPIRV_FIXED_ID_TYPE_F4X4_FUNCTION_POINTER,

    SPIRV_FIXED_ID_TYPE_COMMON_SAMPLER,

    SPIRV_FIXED_ID_TYPE_SAMPLER_2D_IMAGE,
    SPIRV_FIXED_ID_TYPE_SAMPLER_2D,
    SPIRV_FIXED_ID_TYPE_SAMPLER_2D_POINTER,

    SPIRV_FIXED_ID_GLSL_LIBRARY,

    SPIRV_FIXED_ID_END,
};

struct kan_rpl_compiler_builtin_node_t
{
    struct kan_hash_storage_node_t node;
    struct compiler_instance_function_node_t *builtin;
};

struct kan_rpl_compiler_statics_t
{
    kan_allocation_group_t rpl_allocation_group;
    kan_allocation_group_t rpl_meta_allocation_group;
    kan_allocation_group_t rpl_compiler_allocation_group;
    kan_allocation_group_t rpl_compiler_builtin_hash_allocation_group;
    kan_allocation_group_t rpl_compiler_context_allocation_group;
    kan_allocation_group_t rpl_compiler_instance_allocation_group;

    kan_interned_string_t interned_fill;
    kan_interned_string_t interned_wireframe;
    kan_interned_string_t interned_back;

    kan_interned_string_t interned_polygon_mode;
    kan_interned_string_t interned_cull_mode;
    kan_interned_string_t interned_depth_test;
    kan_interned_string_t interned_depth_write;

    kan_interned_string_t interned_nearest;
    kan_interned_string_t interned_linear;
    kan_interned_string_t interned_repeat;
    kan_interned_string_t interned_mirrored_repeat;
    kan_interned_string_t interned_clamp_to_edge;
    kan_interned_string_t interned_clamp_to_border;
    kan_interned_string_t interned_mirror_clamp_to_edge;
    kan_interned_string_t interned_mirror_clamp_to_border;

    kan_interned_string_t interned_mag_filter;
    kan_interned_string_t interned_min_filter;
    kan_interned_string_t interned_mip_map_mode;
    kan_interned_string_t interned_address_mode_u;
    kan_interned_string_t interned_address_mode_v;
    kan_interned_string_t interned_address_mode_w;

    kan_interned_string_t interned_void;

    struct inbuilt_vector_type_t type_f1;
    struct compiler_instance_declaration_node_t type_f1_constructor_signatures[1u];

    struct inbuilt_vector_type_t type_f2;
    struct compiler_instance_declaration_node_t type_f2_constructor_signatures[2u];

    struct inbuilt_vector_type_t type_f3;
    struct compiler_instance_declaration_node_t type_f3_constructor_signatures[3u];

    struct inbuilt_vector_type_t type_f4;
    struct compiler_instance_declaration_node_t type_f4_constructor_signatures[4u];

    struct inbuilt_vector_type_t type_i1;
    struct compiler_instance_declaration_node_t type_i1_constructor_signatures[1u];

    struct inbuilt_vector_type_t type_i2;
    struct compiler_instance_declaration_node_t type_i2_constructor_signatures[2u];

    struct inbuilt_vector_type_t type_i3;
    struct compiler_instance_declaration_node_t type_i3_constructor_signatures[3u];

    struct inbuilt_vector_type_t type_i4;
    struct compiler_instance_declaration_node_t type_i4_constructor_signatures[4u];

    struct inbuilt_vector_type_t *vector_types[8u];
    struct inbuilt_vector_type_t *floating_vector_types[4u];
    struct inbuilt_vector_type_t *integer_vector_types[4u];

    struct inbuilt_matrix_type_t type_f3x3;
    struct compiler_instance_declaration_node_t type_f3x3_constructor_signatures[3u];

    struct inbuilt_matrix_type_t type_f4x4;
    struct compiler_instance_declaration_node_t type_f4x4_constructor_signatures[4u];

    struct inbuilt_matrix_type_t *matrix_types[2u];

    struct compiler_instance_declaration_node_t *sampler_2d_call_signature_first_element;
    struct compiler_instance_declaration_node_t sampler_2d_call_signature_location;

    struct kan_hash_storage_t builtin_hash_storage;

    struct compiler_instance_function_node_t builtin_vertex_stage_output_position;
    struct compiler_instance_declaration_node_t builtin_vertex_stage_output_position_arguments[1u];

    struct compiler_instance_function_node_t builtin_pi;

    struct compiler_instance_function_node_t builtin_i1_to_f1;
    struct compiler_instance_declaration_node_t builtin_i1_to_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_i2_to_f2;
    struct compiler_instance_declaration_node_t builtin_i2_to_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_i3_to_f3;
    struct compiler_instance_declaration_node_t builtin_i3_to_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_i4_to_f4;
    struct compiler_instance_declaration_node_t builtin_i4_to_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_f1_to_i1;
    struct compiler_instance_declaration_node_t builtin_f1_to_i1_arguments[1u];

    struct compiler_instance_function_node_t builtin_f2_to_i2;
    struct compiler_instance_declaration_node_t builtin_f2_to_i2_arguments[1u];

    struct compiler_instance_function_node_t builtin_f3_to_i3;
    struct compiler_instance_declaration_node_t builtin_f3_to_i3_arguments[1u];

    struct compiler_instance_function_node_t builtin_f4_to_i4;
    struct compiler_instance_declaration_node_t builtin_f4_to_i4_arguments[1u];

    struct compiler_instance_function_node_t builtin_round_f1;
    struct compiler_instance_declaration_node_t builtin_round_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_round_f2;
    struct compiler_instance_declaration_node_t builtin_round_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_round_f3;
    struct compiler_instance_declaration_node_t builtin_round_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_round_f4;
    struct compiler_instance_declaration_node_t builtin_round_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_round_even_f1;
    struct compiler_instance_declaration_node_t builtin_round_even_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_round_even_f2;
    struct compiler_instance_declaration_node_t builtin_round_even_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_round_even_f3;
    struct compiler_instance_declaration_node_t builtin_round_even_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_round_even_f4;
    struct compiler_instance_declaration_node_t builtin_round_even_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_trunc_f1;
    struct compiler_instance_declaration_node_t builtin_trunc_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_trunc_f2;
    struct compiler_instance_declaration_node_t builtin_trunc_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_trunc_f3;
    struct compiler_instance_declaration_node_t builtin_trunc_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_trunc_f4;
    struct compiler_instance_declaration_node_t builtin_trunc_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_abs_f1;
    struct compiler_instance_declaration_node_t builtin_abs_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_abs_f2;
    struct compiler_instance_declaration_node_t builtin_abs_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_abs_f3;
    struct compiler_instance_declaration_node_t builtin_abs_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_abs_f4;
    struct compiler_instance_declaration_node_t builtin_abs_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_abs_i1;
    struct compiler_instance_declaration_node_t builtin_abs_i1_arguments[1u];

    struct compiler_instance_function_node_t builtin_abs_i2;
    struct compiler_instance_declaration_node_t builtin_abs_i2_arguments[1u];

    struct compiler_instance_function_node_t builtin_abs_i3;
    struct compiler_instance_declaration_node_t builtin_abs_i3_arguments[1u];

    struct compiler_instance_function_node_t builtin_abs_i4;
    struct compiler_instance_declaration_node_t builtin_abs_i4_arguments[1u];

    struct compiler_instance_function_node_t builtin_sign_f1;
    struct compiler_instance_declaration_node_t builtin_sign_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_sign_f2;
    struct compiler_instance_declaration_node_t builtin_sign_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_sign_f3;
    struct compiler_instance_declaration_node_t builtin_sign_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_sign_f4;
    struct compiler_instance_declaration_node_t builtin_sign_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_sign_i1;
    struct compiler_instance_declaration_node_t builtin_sign_i1_arguments[1u];

    struct compiler_instance_function_node_t builtin_sign_i2;
    struct compiler_instance_declaration_node_t builtin_sign_i2_arguments[1u];

    struct compiler_instance_function_node_t builtin_sign_i3;
    struct compiler_instance_declaration_node_t builtin_sign_i3_arguments[1u];

    struct compiler_instance_function_node_t builtin_sign_i4;
    struct compiler_instance_declaration_node_t builtin_sign_i4_arguments[1u];

    struct compiler_instance_function_node_t builtin_floor_f1;
    struct compiler_instance_declaration_node_t builtin_floor_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_floor_f2;
    struct compiler_instance_declaration_node_t builtin_floor_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_floor_f3;
    struct compiler_instance_declaration_node_t builtin_floor_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_floor_f4;
    struct compiler_instance_declaration_node_t builtin_floor_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_ceil_f1;
    struct compiler_instance_declaration_node_t builtin_ceil_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_ceil_f2;
    struct compiler_instance_declaration_node_t builtin_ceil_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_ceil_f3;
    struct compiler_instance_declaration_node_t builtin_ceil_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_ceil_f4;
    struct compiler_instance_declaration_node_t builtin_ceil_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_fract_f1;
    struct compiler_instance_declaration_node_t builtin_fract_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_fract_f2;
    struct compiler_instance_declaration_node_t builtin_fract_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_fract_f3;
    struct compiler_instance_declaration_node_t builtin_fract_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_fract_f4;
    struct compiler_instance_declaration_node_t builtin_fract_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_sin_f1;
    struct compiler_instance_declaration_node_t builtin_sin_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_sin_f2;
    struct compiler_instance_declaration_node_t builtin_sin_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_sin_f3;
    struct compiler_instance_declaration_node_t builtin_sin_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_sin_f4;
    struct compiler_instance_declaration_node_t builtin_sin_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_cos_f1;
    struct compiler_instance_declaration_node_t builtin_cos_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_cos_f2;
    struct compiler_instance_declaration_node_t builtin_cos_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_cos_f3;
    struct compiler_instance_declaration_node_t builtin_cos_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_cos_f4;
    struct compiler_instance_declaration_node_t builtin_cos_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_tan_f1;
    struct compiler_instance_declaration_node_t builtin_tan_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_tan_f2;
    struct compiler_instance_declaration_node_t builtin_tan_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_tan_f3;
    struct compiler_instance_declaration_node_t builtin_tan_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_tan_f4;
    struct compiler_instance_declaration_node_t builtin_tan_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_asin_f1;
    struct compiler_instance_declaration_node_t builtin_asin_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_asin_f2;
    struct compiler_instance_declaration_node_t builtin_asin_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_asin_f3;
    struct compiler_instance_declaration_node_t builtin_asin_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_asin_f4;
    struct compiler_instance_declaration_node_t builtin_asin_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_acos_f1;
    struct compiler_instance_declaration_node_t builtin_acos_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_acos_f2;
    struct compiler_instance_declaration_node_t builtin_acos_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_acos_f3;
    struct compiler_instance_declaration_node_t builtin_acos_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_acos_f4;
    struct compiler_instance_declaration_node_t builtin_acos_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_atan_f1;
    struct compiler_instance_declaration_node_t builtin_atan_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_atan_f2;
    struct compiler_instance_declaration_node_t builtin_atan_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_atan_f3;
    struct compiler_instance_declaration_node_t builtin_atan_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_atan_f4;
    struct compiler_instance_declaration_node_t builtin_atan_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_sinh_f1;
    struct compiler_instance_declaration_node_t builtin_sinh_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_sinh_f2;
    struct compiler_instance_declaration_node_t builtin_sinh_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_sinh_f3;
    struct compiler_instance_declaration_node_t builtin_sinh_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_sinh_f4;
    struct compiler_instance_declaration_node_t builtin_sinh_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_cosh_f1;
    struct compiler_instance_declaration_node_t builtin_cosh_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_cosh_f2;
    struct compiler_instance_declaration_node_t builtin_cosh_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_cosh_f3;
    struct compiler_instance_declaration_node_t builtin_cosh_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_cosh_f4;
    struct compiler_instance_declaration_node_t builtin_cosh_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_tanh_f1;
    struct compiler_instance_declaration_node_t builtin_tanh_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_tanh_f2;
    struct compiler_instance_declaration_node_t builtin_tanh_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_tanh_f3;
    struct compiler_instance_declaration_node_t builtin_tanh_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_tanh_f4;
    struct compiler_instance_declaration_node_t builtin_tanh_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_asinh_f1;
    struct compiler_instance_declaration_node_t builtin_asinh_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_asinh_f2;
    struct compiler_instance_declaration_node_t builtin_asinh_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_asinh_f3;
    struct compiler_instance_declaration_node_t builtin_asinh_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_asinh_f4;
    struct compiler_instance_declaration_node_t builtin_asinh_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_acosh_f1;
    struct compiler_instance_declaration_node_t builtin_acosh_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_acosh_f2;
    struct compiler_instance_declaration_node_t builtin_acosh_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_acosh_f3;
    struct compiler_instance_declaration_node_t builtin_acosh_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_acosh_f4;
    struct compiler_instance_declaration_node_t builtin_acosh_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_atanh_f1;
    struct compiler_instance_declaration_node_t builtin_atanh_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_atanh_f2;
    struct compiler_instance_declaration_node_t builtin_atanh_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_atanh_f3;
    struct compiler_instance_declaration_node_t builtin_atanh_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_atanh_f4;
    struct compiler_instance_declaration_node_t builtin_atanh_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_atan2_f1;
    struct compiler_instance_declaration_node_t builtin_atan2_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_atan2_f2;
    struct compiler_instance_declaration_node_t builtin_atan2_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_atan2_f3;
    struct compiler_instance_declaration_node_t builtin_atan2_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_atan2_f4;
    struct compiler_instance_declaration_node_t builtin_atan2_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_pow_f1;
    struct compiler_instance_declaration_node_t builtin_pow_f1_arguments[2u];

    struct compiler_instance_function_node_t builtin_pow_f2;
    struct compiler_instance_declaration_node_t builtin_pow_f2_arguments[2u];

    struct compiler_instance_function_node_t builtin_pow_f3;
    struct compiler_instance_declaration_node_t builtin_pow_f3_arguments[2u];

    struct compiler_instance_function_node_t builtin_pow_f4;
    struct compiler_instance_declaration_node_t builtin_pow_f4_arguments[2u];

    struct compiler_instance_function_node_t builtin_exp_f1;
    struct compiler_instance_declaration_node_t builtin_exp_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_exp_f2;
    struct compiler_instance_declaration_node_t builtin_exp_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_exp_f3;
    struct compiler_instance_declaration_node_t builtin_exp_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_exp_f4;
    struct compiler_instance_declaration_node_t builtin_exp_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_log_f1;
    struct compiler_instance_declaration_node_t builtin_log_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_log_f2;
    struct compiler_instance_declaration_node_t builtin_log_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_log_f3;
    struct compiler_instance_declaration_node_t builtin_log_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_log_f4;
    struct compiler_instance_declaration_node_t builtin_log_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_exp2_f1;
    struct compiler_instance_declaration_node_t builtin_exp2_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_exp2_f2;
    struct compiler_instance_declaration_node_t builtin_exp2_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_exp2_f3;
    struct compiler_instance_declaration_node_t builtin_exp2_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_exp2_f4;
    struct compiler_instance_declaration_node_t builtin_exp2_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_log2_f1;
    struct compiler_instance_declaration_node_t builtin_log2_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_log2_f2;
    struct compiler_instance_declaration_node_t builtin_log2_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_log2_f3;
    struct compiler_instance_declaration_node_t builtin_log2_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_log2_f4;
    struct compiler_instance_declaration_node_t builtin_log2_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_sqrt_f1;
    struct compiler_instance_declaration_node_t builtin_sqrt_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_sqrt_f2;
    struct compiler_instance_declaration_node_t builtin_sqrt_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_sqrt_f3;
    struct compiler_instance_declaration_node_t builtin_sqrt_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_sqrt_f4;
    struct compiler_instance_declaration_node_t builtin_sqrt_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_inverse_sqrt_f1;
    struct compiler_instance_declaration_node_t builtin_inverse_sqrt_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_inverse_sqrt_f2;
    struct compiler_instance_declaration_node_t builtin_inverse_sqrt_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_inverse_sqrt_f3;
    struct compiler_instance_declaration_node_t builtin_inverse_sqrt_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_inverse_sqrt_f4;
    struct compiler_instance_declaration_node_t builtin_inverse_sqrt_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_determinant_f3x3;
    struct compiler_instance_declaration_node_t builtin_determinant_f3x3_arguments[1u];

    struct compiler_instance_function_node_t builtin_determinant_f4x4;
    struct compiler_instance_declaration_node_t builtin_determinant_f4x4_arguments[1u];

    struct compiler_instance_function_node_t builtin_inverse_matrix_f3x3;
    struct compiler_instance_declaration_node_t builtin_inverse_matrix_f3x3_arguments[1u];

    struct compiler_instance_function_node_t builtin_inverse_matrix_f4x4;
    struct compiler_instance_declaration_node_t builtin_inverse_matrix_f4x4_arguments[1u];

    struct compiler_instance_function_node_t builtin_transpose_matrix_f3x3;
    struct compiler_instance_declaration_node_t builtin_transpose_matrix_f3x3_arguments[1u];

    struct compiler_instance_function_node_t builtin_transpose_matrix_f4x4;
    struct compiler_instance_declaration_node_t builtin_transpose_matrix_f4x4_arguments[1u];

    struct compiler_instance_function_node_t builtin_min_f1;
    struct compiler_instance_declaration_node_t builtin_min_f1_arguments[2u];

    struct compiler_instance_function_node_t builtin_min_f2;
    struct compiler_instance_declaration_node_t builtin_min_f2_arguments[2u];

    struct compiler_instance_function_node_t builtin_min_f3;
    struct compiler_instance_declaration_node_t builtin_min_f3_arguments[2u];

    struct compiler_instance_function_node_t builtin_min_f4;
    struct compiler_instance_declaration_node_t builtin_min_f4_arguments[2u];

    struct compiler_instance_function_node_t builtin_min_i1;
    struct compiler_instance_declaration_node_t builtin_min_i1_arguments[2u];

    struct compiler_instance_function_node_t builtin_min_i2;
    struct compiler_instance_declaration_node_t builtin_min_i2_arguments[2u];

    struct compiler_instance_function_node_t builtin_min_i3;
    struct compiler_instance_declaration_node_t builtin_min_i3_arguments[2u];

    struct compiler_instance_function_node_t builtin_min_i4;
    struct compiler_instance_declaration_node_t builtin_min_i4_arguments[2u];

    struct compiler_instance_function_node_t builtin_max_f1;
    struct compiler_instance_declaration_node_t builtin_max_f1_arguments[2u];

    struct compiler_instance_function_node_t builtin_max_f2;
    struct compiler_instance_declaration_node_t builtin_max_f2_arguments[2u];

    struct compiler_instance_function_node_t builtin_max_f3;
    struct compiler_instance_declaration_node_t builtin_max_f3_arguments[2u];

    struct compiler_instance_function_node_t builtin_max_f4;
    struct compiler_instance_declaration_node_t builtin_max_f4_arguments[2u];

    struct compiler_instance_function_node_t builtin_max_i1;
    struct compiler_instance_declaration_node_t builtin_max_i1_arguments[2u];

    struct compiler_instance_function_node_t builtin_max_i2;
    struct compiler_instance_declaration_node_t builtin_max_i2_arguments[2u];

    struct compiler_instance_function_node_t builtin_max_i3;
    struct compiler_instance_declaration_node_t builtin_max_i3_arguments[2u];

    struct compiler_instance_function_node_t builtin_max_i4;
    struct compiler_instance_declaration_node_t builtin_max_i4_arguments[2u];

    struct compiler_instance_function_node_t builtin_clamp_f1;
    struct compiler_instance_declaration_node_t builtin_clamp_f1_arguments[3u];

    struct compiler_instance_function_node_t builtin_clamp_f2;
    struct compiler_instance_declaration_node_t builtin_clamp_f2_arguments[3u];

    struct compiler_instance_function_node_t builtin_clamp_f3;
    struct compiler_instance_declaration_node_t builtin_clamp_f3_arguments[3u];

    struct compiler_instance_function_node_t builtin_clamp_f4;
    struct compiler_instance_declaration_node_t builtin_clamp_f4_arguments[3u];

    struct compiler_instance_function_node_t builtin_clamp_i1;
    struct compiler_instance_declaration_node_t builtin_clamp_i1_arguments[3u];

    struct compiler_instance_function_node_t builtin_clamp_i2;
    struct compiler_instance_declaration_node_t builtin_clamp_i2_arguments[3u];

    struct compiler_instance_function_node_t builtin_clamp_i3;
    struct compiler_instance_declaration_node_t builtin_clamp_i3_arguments[3u];

    struct compiler_instance_function_node_t builtin_clamp_i4;
    struct compiler_instance_declaration_node_t builtin_clamp_i4_arguments[3u];

    struct compiler_instance_function_node_t builtin_mix_f1;
    struct compiler_instance_declaration_node_t builtin_mix_f1_arguments[3u];

    struct compiler_instance_function_node_t builtin_mix_f2;
    struct compiler_instance_declaration_node_t builtin_mix_f2_arguments[3u];

    struct compiler_instance_function_node_t builtin_mix_f3;
    struct compiler_instance_declaration_node_t builtin_mix_f3_arguments[3u];

    struct compiler_instance_function_node_t builtin_mix_f4;
    struct compiler_instance_declaration_node_t builtin_mix_f4_arguments[3u];

    struct compiler_instance_function_node_t builtin_fma_f1;
    struct compiler_instance_declaration_node_t builtin_fma_f1_arguments[3u];

    struct compiler_instance_function_node_t builtin_fma_f2;
    struct compiler_instance_declaration_node_t builtin_fma_f2_arguments[3u];

    struct compiler_instance_function_node_t builtin_fma_f3;
    struct compiler_instance_declaration_node_t builtin_fma_f3_arguments[3u];

    struct compiler_instance_function_node_t builtin_fma_f4;
    struct compiler_instance_declaration_node_t builtin_fma_f4_arguments[3u];

    struct compiler_instance_function_node_t builtin_length_f1;
    struct compiler_instance_declaration_node_t builtin_length_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_length_f2;
    struct compiler_instance_declaration_node_t builtin_length_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_length_f3;
    struct compiler_instance_declaration_node_t builtin_length_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_length_f4;
    struct compiler_instance_declaration_node_t builtin_length_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_distance_f1;
    struct compiler_instance_declaration_node_t builtin_distance_f1_arguments[2u];

    struct compiler_instance_function_node_t builtin_distance_f2;
    struct compiler_instance_declaration_node_t builtin_distance_f2_arguments[2u];

    struct compiler_instance_function_node_t builtin_distance_f3;
    struct compiler_instance_declaration_node_t builtin_distance_f3_arguments[2u];

    struct compiler_instance_function_node_t builtin_distance_f4;
    struct compiler_instance_declaration_node_t builtin_distance_f4_arguments[2u];

    struct compiler_instance_function_node_t builtin_cross_f3;
    struct compiler_instance_declaration_node_t builtin_cross_f3_arguments[2u];

    struct compiler_instance_function_node_t builtin_dot_f1;
    struct compiler_instance_declaration_node_t builtin_dot_f1_arguments[2u];

    struct compiler_instance_function_node_t builtin_dot_f2;
    struct compiler_instance_declaration_node_t builtin_dot_f2_arguments[2u];

    struct compiler_instance_function_node_t builtin_dot_f3;
    struct compiler_instance_declaration_node_t builtin_dot_f3_arguments[2u];

    struct compiler_instance_function_node_t builtin_dot_f4;
    struct compiler_instance_declaration_node_t builtin_dot_f4_arguments[2u];

    struct compiler_instance_function_node_t builtin_normalize_f1;
    struct compiler_instance_declaration_node_t builtin_normalize_f1_arguments[1u];

    struct compiler_instance_function_node_t builtin_normalize_f2;
    struct compiler_instance_declaration_node_t builtin_normalize_f2_arguments[1u];

    struct compiler_instance_function_node_t builtin_normalize_f3;
    struct compiler_instance_declaration_node_t builtin_normalize_f3_arguments[1u];

    struct compiler_instance_function_node_t builtin_normalize_f4;
    struct compiler_instance_declaration_node_t builtin_normalize_f4_arguments[1u];

    struct compiler_instance_function_node_t builtin_reflect_f1;
    struct compiler_instance_declaration_node_t builtin_reflect_f1_arguments[2u];

    struct compiler_instance_function_node_t builtin_reflect_f2;
    struct compiler_instance_declaration_node_t builtin_reflect_f2_arguments[2u];

    struct compiler_instance_function_node_t builtin_reflect_f3;
    struct compiler_instance_declaration_node_t builtin_reflect_f3_arguments[2u];

    struct compiler_instance_function_node_t builtin_reflect_f4;
    struct compiler_instance_declaration_node_t builtin_reflect_f4_arguments[2u];

    struct compiler_instance_function_node_t builtin_refract_f1;
    struct compiler_instance_declaration_node_t builtin_refract_f1_arguments[3u];

    struct compiler_instance_function_node_t builtin_refract_f2;
    struct compiler_instance_declaration_node_t builtin_refract_f2_arguments[3u];

    struct compiler_instance_function_node_t builtin_refract_f3;
    struct compiler_instance_declaration_node_t builtin_refract_f3_arguments[3u];

    struct compiler_instance_function_node_t builtin_refract_f4;
    struct compiler_instance_declaration_node_t builtin_refract_f4_arguments[3u];

    struct compiler_instance_function_node_t builtin_expand_f3_to_f4;
    struct compiler_instance_declaration_node_t builtin_expand_f3_to_f4_arguments[2u];

    struct compiler_instance_function_node_t builtin_crop_f4_to_f3;
    struct compiler_instance_declaration_node_t builtin_crop_f4_to_f3_arguments[2u];

    struct compiler_instance_function_node_t builtin_crop_f4x4_to_f3x3;
    struct compiler_instance_declaration_node_t builtin_crop_f4x4_to_f3x3_arguments[2u];
};

extern struct kan_rpl_compiler_statics_t kan_rpl_compiler_statics;

/// \details Just internal define to make code shorter.
#define STATICS kan_rpl_compiler_statics

void kan_rpl_compiler_ensure_statics_initialized (void);

static inline struct inbuilt_vector_type_t *find_inbuilt_vector_type (kan_interned_string_t name)
{
    for (uint64_t index = 0u;
         index < sizeof (kan_rpl_compiler_statics.vector_types) / sizeof (kan_rpl_compiler_statics.vector_types[0u]);
         ++index)
    {
        if (kan_rpl_compiler_statics.vector_types[index]->name == name)
        {
            return kan_rpl_compiler_statics.vector_types[index];
        }
    }

    return NULL;
}

static inline struct inbuilt_matrix_type_t *find_inbuilt_matrix_type (kan_interned_string_t name)
{
    for (uint64_t index = 0u;
         index < sizeof (kan_rpl_compiler_statics.matrix_types) / sizeof (kan_rpl_compiler_statics.matrix_types[0u]);
         ++index)
    {
        if (kan_rpl_compiler_statics.matrix_types[index]->name == name)
        {
            return kan_rpl_compiler_statics.matrix_types[index];
        }
    }

    return NULL;
}

static inline struct compiler_instance_function_node_t *find_builtin_function (kan_interned_string_t name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&STATICS.builtin_hash_storage, (uint64_t) name);
    struct kan_rpl_compiler_builtin_node_t *node = (struct kan_rpl_compiler_builtin_node_t *) bucket->first;
    const struct kan_rpl_compiler_builtin_node_t *node_end =
        (struct kan_rpl_compiler_builtin_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->node.hash == (uint64_t) name)
        {
            return node->builtin;
        }

        node = (struct kan_rpl_compiler_builtin_node_t *) node->node.list_node.next;
    }

    return NULL;
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
    const uint64_t new_length = buffer->length > 0u ? buffer->length + 1u + sub_name_length : sub_name_length;
    const uint64_t dot_position = buffer->length;
    const uint64_t sub_name_position = dot_position == 0u ? 0u : dot_position + 1u;
    flattening_name_generation_buffer_reset (buffer, new_length);

    if (dot_position != 0u && dot_position < buffer->length)
    {
        buffer->buffer[dot_position] = '.';
    }

    if (sub_name_position < buffer->length)
    {
        const uint64_t to_copy = buffer->length - sub_name_position;
        memcpy (&buffer->buffer[sub_name_position], name, to_copy);
    }
}

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

static inline void calculate_full_type_definition_size_and_alignment (
    struct compiler_instance_full_type_definition_t *definition,
    uint64_t dimension_offset,
    uint64_t *size,
    uint64_t *alignment)
{
    *size = 0u;
    *alignment = 0u;

    if (definition->if_vector)
    {
        *size = inbuilt_type_item_size[definition->if_vector->item] * definition->if_vector->items_count;
        *alignment = inbuilt_type_item_size[definition->if_vector->item];
    }
    else if (definition->if_matrix)
    {
        *size = inbuilt_type_item_size[definition->if_matrix->item] * definition->if_matrix->rows *
                definition->if_matrix->columns;
        *alignment = inbuilt_type_item_size[definition->if_matrix->item];
    }
    else if (definition->if_struct)
    {
        *size = definition->if_struct->size;
        *alignment = definition->if_struct->alignment;
    }

    for (uint64_t dimension = dimension_offset; dimension < definition->array_dimensions_count; ++dimension)
    {
        *size *= definition->array_dimensions[dimension];
    }
}
