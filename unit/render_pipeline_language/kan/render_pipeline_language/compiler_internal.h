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
#include <kan/container/trivial_string_buffer.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/render_pipeline_language/compiler.h>
#include <kan/threading/atomic.h>

/// \brief SPIRV used 32-bit integers for everything inside bytecode.
typedef uint32_t spirv_size_t;

/// \brief Unsigned integer type for SPIRV bytecode.
typedef uint32_t spirv_signed_literal_t;

struct rpl_compiler_context_option_value_t
{
    kan_interned_string_t name;
    enum kan_rpl_option_scope_t scope;
    enum kan_rpl_option_type_t type;

    union
    {
        kan_bool_t flag_value;
        kan_rpl_unsigned_int_literal_t count_value;
    };
};

struct rpl_compiler_context_t
{
    enum kan_rpl_pipeline_type_t pipeline_type;
    kan_interned_string_t log_name;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct rpl_compiler_context_option_value_t)
    struct kan_dynamic_array_t option_values;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_intermediate_t *)
    struct kan_dynamic_array_t modules;

    struct kan_stack_group_allocator_t resolve_allocator;
    struct kan_trivial_string_buffer_t name_generation_buffer;
};

struct compiler_instance_setting_node_t
{
    struct compiler_instance_setting_node_t *next;
    kan_interned_string_t name;
    kan_rpl_size_t block;
    enum kan_rpl_setting_type_t type;

    union
    {
        kan_bool_t flag;
        kan_rpl_signed_int_literal_t integer;
        float floating;
        kan_interned_string_t string;
    };

    /// \details Conditional expression if it is not KAN_RPL_EXPRESSION_NODE_TYPE_NOPE.
    struct kan_rpl_expression_t conditional;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    kan_rpl_size_t source_line;
};

struct compiler_instance_full_type_definition_t
{
    struct inbuilt_vector_type_t *if_vector;
    struct inbuilt_matrix_type_t *if_matrix;
    struct compiler_instance_struct_node_t *if_struct;

    kan_instance_size_t array_dimensions_count;
    kan_rpl_size_t *array_dimensions;
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

    kan_instance_size_t offset;
    kan_instance_size_t size;
    kan_instance_size_t alignment;

    kan_instance_size_t meta_count;
    kan_interned_string_t *meta;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    kan_rpl_size_t source_line;
};

struct compiler_instance_struct_node_t
{
    struct compiler_instance_struct_node_t *next;
    kan_interned_string_t name;
    kan_instance_size_t size;
    kan_instance_size_t alignment;
    struct compiler_instance_declaration_node_t *first_field;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    kan_rpl_size_t source_line;

    spirv_size_t spirv_id_value;
    spirv_size_t spirv_id_function_pointer;
};

#define INVALID_LOCATION KAN_INT_MAX (kan_rpl_size_t)
#define INVALID_SET KAN_INT_MAX (kan_rpl_size_t)
#define INVALID_BINDING KAN_INT_MAX (kan_rpl_size_t)

struct binding_location_assignment_counter_t
{
    kan_rpl_size_t next_attribute_buffer_binding;
    kan_rpl_size_t next_arbitrary_stable_buffer_binding;
    kan_rpl_size_t next_arbitrary_unstable_buffer_binding;
    kan_rpl_size_t next_attribute_location;
    kan_rpl_size_t next_vertex_output_location;
    kan_rpl_size_t next_fragment_output_location;
};

struct compiler_instance_buffer_flattened_declaration_t
{
    struct compiler_instance_buffer_flattened_declaration_t *next;
    struct compiler_instance_declaration_node_t *source_declaration;
    kan_interned_string_t readable_name;
    kan_rpl_size_t location;

    spirv_size_t spirv_id_input;
    spirv_size_t spirv_id_output;
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
    enum kan_rpl_set_t set;
    enum kan_rpl_buffer_type_t type;
    kan_bool_t used;

    kan_instance_size_t size;
    kan_instance_size_t alignment;
    struct compiler_instance_declaration_node_t *first_field;

    kan_instance_size_t binding;
    kan_bool_t stable_binding;

    struct compiler_instance_buffer_flattening_graph_node_t *flattening_graph_base;
    struct compiler_instance_buffer_flattened_declaration_t *first_flattened_declaration;
    struct compiler_instance_buffer_flattened_declaration_t *last_flattened_declaration;

    spirv_size_t structured_variable_spirv_id;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    kan_rpl_size_t source_line;
};

struct compiler_instance_sampler_node_t
{
    struct compiler_instance_sampler_node_t *next;
    kan_interned_string_t name;
    enum kan_rpl_set_t set;
    enum kan_rpl_sampler_type_t type;
    kan_bool_t used;

    kan_instance_size_t binding;
    struct compiler_instance_setting_node_t *first_setting;

    spirv_size_t variable_spirv_id;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    kan_rpl_size_t source_line;
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
    kan_instance_size_t access_chain_length;
    kan_instance_size_t *access_chain_indices;
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
    spirv_size_t spirv_id;
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

    spirv_size_t spirv_label_break;
    spirv_size_t spirv_label_continue;
};

struct compiler_instance_while_suffix_t
{
    struct compiler_instance_expression_node_t *condition;
    struct compiler_instance_expression_node_t *body;

    spirv_size_t spirv_label_break;
    spirv_size_t spirv_label_continue;
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
        kan_rpl_signed_int_literal_t integer_literal;
        float floating_literal;
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
    kan_rpl_size_t source_line;
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

    spirv_size_t spirv_id;
    spirv_size_t spirv_external_library_id;
    spirv_size_t spirv_external_instruction_id;
    const struct spirv_generation_function_type_t *spirv_function_type;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    kan_rpl_size_t source_line;
};

struct rpl_compiler_instance_t
{
    enum kan_rpl_pipeline_type_t pipeline_type;
    kan_interned_string_t context_log_name;
    struct kan_stack_group_allocator_t resolve_allocator;

    kan_instance_size_t entry_point_count;
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

    struct kan_trivial_string_buffer_t resolve_name_generation_buffer;
};

enum inbuilt_type_item_t
{
    INBUILT_TYPE_ITEM_FLOAT = 0u,
    INBUILT_TYPE_ITEM_INTEGER,
};

static kan_instance_size_t inbuilt_type_item_size[] = {
    4u,
    4u,
};

struct inbuilt_vector_type_t
{
    kan_interned_string_t name;
    enum inbuilt_type_item_t item;
    kan_instance_size_t items_count;
    enum kan_rpl_meta_variable_type_t meta_type;
    struct compiler_instance_declaration_node_t *constructor_signature;

    spirv_size_t spirv_id;
    spirv_size_t spirv_id_input_pointer;
    spirv_size_t spirv_id_output_pointer;
    spirv_size_t spirv_id_function_pointer;
};

struct inbuilt_matrix_type_t
{
    kan_interned_string_t name;
    enum inbuilt_type_item_t item;
    kan_instance_size_t rows;
    kan_instance_size_t columns;
    enum kan_rpl_meta_variable_type_t meta_type;
    struct compiler_instance_declaration_node_t *constructor_signature;

    spirv_size_t spirv_id;
    spirv_size_t spirv_id_input_pointer;
    spirv_size_t spirv_id_output_pointer;
    spirv_size_t spirv_id_function_pointer;
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
    kan_interned_string_t name;
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

    kan_interned_string_t interned_never;
    kan_interned_string_t interned_always;
    kan_interned_string_t interned_equal;
    kan_interned_string_t interned_not_equal;
    kan_interned_string_t interned_less;
    kan_interned_string_t interned_less_or_equal;
    kan_interned_string_t interned_greater;
    kan_interned_string_t interned_greater_or_equal;

    kan_interned_string_t interned_keep;
    kan_interned_string_t interned_replace;
    kan_interned_string_t interned_increment_and_clamp;
    kan_interned_string_t interned_decrement_and_clamp;
    kan_interned_string_t interned_invert;
    kan_interned_string_t interned_increment_and_wrap;
    kan_interned_string_t interned_decrement_and_wrap;

    kan_interned_string_t interned_polygon_mode;
    kan_interned_string_t interned_cull_mode;
    kan_interned_string_t interned_depth_test;
    kan_interned_string_t interned_depth_write;
    kan_interned_string_t interned_depth_bounds_test;
    kan_interned_string_t interned_depth_compare_operation;
    kan_interned_string_t interned_depth_min;
    kan_interned_string_t interned_depth_max;
    kan_interned_string_t interned_stencil_test;
    kan_interned_string_t interned_stencil_front_on_fail;
    kan_interned_string_t interned_stencil_front_on_depth_fail;
    kan_interned_string_t interned_stencil_front_on_pass;
    kan_interned_string_t interned_stencil_front_compare;
    kan_interned_string_t interned_stencil_front_compare_mask;
    kan_interned_string_t interned_stencil_front_write_mask;
    kan_interned_string_t interned_stencil_front_reference;
    kan_interned_string_t interned_stencil_back_on_fail;
    kan_interned_string_t interned_stencil_back_on_depth_fail;
    kan_interned_string_t interned_stencil_back_on_pass;
    kan_interned_string_t interned_stencil_back_compare;
    kan_interned_string_t interned_stencil_back_compare_mask;
    kan_interned_string_t interned_stencil_back_write_mask;
    kan_interned_string_t interned_stencil_back_reference;

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

    kan_interned_string_t interned_zero;
    kan_interned_string_t interned_one;
    kan_interned_string_t interned_source_color;
    kan_interned_string_t interned_one_minus_source_color;
    kan_interned_string_t interned_destination_color;
    kan_interned_string_t interned_one_minus_destination_color;
    kan_interned_string_t interned_source_alpha;
    kan_interned_string_t interned_one_minus_source_alpha;
    kan_interned_string_t interned_destination_alpha;
    kan_interned_string_t interned_one_minus_destination_alpha;
    kan_interned_string_t interned_constant_color;
    kan_interned_string_t interned_one_minus_constant_color;
    kan_interned_string_t interned_constant_alpha;
    kan_interned_string_t interned_one_minus_constant_alpha;
    kan_interned_string_t interned_source_alpha_saturate;

    kan_interned_string_t interned_add;
    kan_interned_string_t interned_subtract;
    kan_interned_string_t interned_reverse_subtract;
    kan_interned_string_t interned_min;
    kan_interned_string_t interned_max;

    kan_interned_string_t interned_color_output_use_blend;
    kan_interned_string_t interned_color_output_write_r;
    kan_interned_string_t interned_color_output_write_g;
    kan_interned_string_t interned_color_output_write_b;
    kan_interned_string_t interned_color_output_write_a;
    kan_interned_string_t interned_color_output_source_color_blend_factor;
    kan_interned_string_t interned_color_output_destination_color_blend_factor;
    kan_interned_string_t interned_color_output_color_blend_operation;
    kan_interned_string_t interned_color_output_source_alpha_blend_factor;
    kan_interned_string_t interned_color_output_destination_alpha_blend_factor;
    kan_interned_string_t interned_color_output_alpha_blend_operation;

    kan_interned_string_t interned_color_blend_constant_r;
    kan_interned_string_t interned_color_blend_constant_g;
    kan_interned_string_t interned_color_blend_constant_b;
    kan_interned_string_t interned_color_blend_constant_a;

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

    struct compiler_instance_function_node_t builtin_step_f1;
    struct compiler_instance_declaration_node_t builtin_step_f1_arguments[2u];

    struct compiler_instance_function_node_t builtin_step_f2;
    struct compiler_instance_declaration_node_t builtin_step_f2_arguments[2u];

    struct compiler_instance_function_node_t builtin_step_f3;
    struct compiler_instance_declaration_node_t builtin_step_f3_arguments[2u];

    struct compiler_instance_function_node_t builtin_step_f4;
    struct compiler_instance_declaration_node_t builtin_step_f4_arguments[2u];

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
    for (kan_loop_size_t index = 0u;
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
    for (kan_loop_size_t index = 0u;
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
        kan_hash_storage_query (&STATICS.builtin_hash_storage, KAN_HASH_OBJECT_POINTER (name));
    struct kan_rpl_compiler_builtin_node_t *node = (struct kan_rpl_compiler_builtin_node_t *) bucket->first;
    const struct kan_rpl_compiler_builtin_node_t *node_end =
        (struct kan_rpl_compiler_builtin_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == name)
        {
            return node->builtin;
        }

        node = (struct kan_rpl_compiler_builtin_node_t *) node->node.list_node.next;
    }

    return NULL;
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
    kan_instance_size_t dimension_offset,
    kan_instance_size_t *size,
    kan_instance_size_t *alignment)
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

    for (kan_loop_size_t dimension = dimension_offset; dimension < definition->array_dimensions_count; ++dimension)
    {
        *size *= definition->array_dimensions[dimension];
    }
}
