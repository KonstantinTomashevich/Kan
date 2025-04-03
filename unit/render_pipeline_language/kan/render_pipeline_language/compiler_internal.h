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
typedef uint32_t spirv_unsigned_literal_t;

/// \brief Signed integer type for SPIRV bytecode.
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

enum compiler_instance_type_class_t
{
    /// \brief Special case for functions that return nothing.
    COMPILER_INSTANCE_TYPE_CLASS_VOID = 0u,

    COMPILER_INSTANCE_TYPE_CLASS_VECTOR,
    COMPILER_INSTANCE_TYPE_CLASS_MATRIX,
    COMPILER_INSTANCE_TYPE_CLASS_STRUCT,
    COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN,
    COMPILER_INSTANCE_TYPE_CLASS_BUFFER,
    COMPILER_INSTANCE_TYPE_CLASS_SAMPLER,
    COMPILER_INSTANCE_TYPE_CLASS_IMAGE,
};

enum compiler_instance_type_flags_t
{
    /// \brief Used for expression output types.
    ///        Indicates that is stored somewhere in pipeline interface, not function scopes.
    /// \details Due to logical pointer model in languages like SPIRV, pointers to interface variables (anything that
    ///          is not in function scope) need additional care when working with them. It makes it particularly
    ///          difficult to use such pointers as writable arguments for functions. Therefore, we currently forbid
    ///          using non-function-scope variables as writable arguments for functions. And to detect this access
    ///          pattern, we need this flag.
    COMPILER_INSTANCE_TYPE_INTERFACE_POINTER = 1u << 0u,
};

struct compiler_instance_type_definition_t
{
    enum compiler_instance_type_class_t class;
    union
    {
        struct inbuilt_vector_type_t *vector_data;
        struct inbuilt_matrix_type_t *matrix_data;
        struct compiler_instance_struct_node_t *struct_data;
        struct compiler_instance_buffer_node_t *buffer_data;
        enum kan_rpl_image_type_t image_type;
    };

    enum kan_rpl_access_class_t access;
    enum compiler_instance_type_flags_t flags;

    kan_bool_t array_size_runtime;
    kan_instance_size_t array_dimensions_count;
    kan_rpl_size_t *array_dimensions;
};

struct compiler_instance_variable_t
{
    kan_interned_string_t name;
    struct compiler_instance_type_definition_t type;
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

#define INVALID_BINDING KAN_INT_MAX (kan_rpl_size_t)

struct binding_location_assignment_counter_t
{
    kan_rpl_size_t next_attribute_container_binding;
    kan_rpl_size_t next_pass_set_binding;
    kan_rpl_size_t next_material_set_binding;
    kan_rpl_size_t next_object_set_binding;
    kan_rpl_size_t next_shared_set_binding;
    kan_rpl_size_t next_attribute_location;
    kan_rpl_size_t next_state_location;
    kan_rpl_size_t next_color_output_location;
};

struct compiler_instance_container_field_stage_node_t
{
    struct compiler_instance_container_field_stage_node_t *next;
    enum kan_rpl_pipeline_stage_t user_stage;
    spirv_size_t spirv_id_input;
    spirv_size_t spirv_id_output;
};

struct compiler_instance_container_field_node_t
{
    struct compiler_instance_container_field_node_t *next;
    struct compiler_instance_variable_t variable;

    kan_instance_size_t location;
    enum kan_rpl_meta_attribute_item_format_t input_item_format;
    struct compiler_instance_container_field_stage_node_t *first_usage_stage;

    kan_instance_size_t size;
    kan_instance_size_t alignment;
    kan_instance_size_t offset_if_input;

    kan_instance_size_t meta_count;
    kan_interned_string_t *meta;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    kan_rpl_size_t source_line;
};

struct compiler_instance_container_node_t
{
    struct compiler_instance_container_node_t *next;
    kan_interned_string_t name;
    enum kan_rpl_container_type_t type;
    kan_bool_t used;

    kan_instance_size_t block_size_if_input;
    kan_instance_size_t block_alignment_if_input;
    kan_instance_size_t binding_if_input;
    struct compiler_instance_container_field_node_t *first_field;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    kan_rpl_size_t source_line;
};

struct compiler_instance_buffer_node_t
{
    struct compiler_instance_buffer_node_t *next;
    kan_interned_string_t name;
    enum kan_rpl_set_t set;
    enum kan_rpl_buffer_type_t type;
    kan_bool_t used;

    kan_instance_size_t main_size;
    kan_instance_size_t tail_item_size;
    kan_instance_size_t alignment;
    struct compiler_instance_declaration_node_t *first_field;
    kan_instance_size_t binding;

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
    kan_bool_t used;

    kan_instance_size_t binding;

    spirv_size_t variable_spirv_id;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    kan_rpl_size_t source_line;
};

struct compiler_instance_image_node_t
{
    struct compiler_instance_image_node_t *next;
    kan_interned_string_t name;
    enum kan_rpl_set_t set;
    enum kan_rpl_image_type_t type;
    kan_rpl_size_t array_size;

    kan_bool_t used;
    kan_instance_size_t binding;

    spirv_size_t variable_spirv_id;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    kan_rpl_size_t source_line;
};

enum compiler_instance_expression_type_t
{
    COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_BUFFER_REFERENCE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_SAMPLER_REFERENCE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_REFERENCE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_VARIABLE_REFERENCE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCTURED_ACCESS,
    COMPILER_INSTANCE_EXPRESSION_TYPE_SWIZZLE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_CONTAINER_FIELD_ACCESS_INPUT,
    COMPILER_INSTANCE_EXPRESSION_TYPE_CONTAINER_FIELD_ACCESS_OUTPUT,
    COMPILER_INSTANCE_EXPRESSION_TYPE_FLOATING_LITERAL,
    COMPILER_INSTANCE_EXPRESSION_TYPE_UNSIGNED_LITERAL,
    COMPILER_INSTANCE_EXPRESSION_TYPE_SIGNED_LITERAL,
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
    COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_SAMPLE,
    COMPILER_INSTANCE_EXPRESSION_TYPE_IMAGE_SAMPLE_DREF,
    COMPILER_INSTANCE_EXPRESSION_TYPE_VECTOR_CONSTRUCTOR,
    COMPILER_INSTANCE_EXPRESSION_TYPE_MATRIX_CONSTRUCTOR,
    COMPILER_INSTANCE_EXPRESSION_TYPE_STRUCT_CONSTRUCTOR,
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

#define SWIZZLE_MAX_ITEMS 4u

struct compiler_instance_swizzle_suffix_t
{
    struct compiler_instance_expression_node_t *input;
    uint8_t items_count;
    uint8_t items[SWIZZLE_MAX_ITEMS];
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

struct compiler_instance_image_sample_suffix_t
{
    struct compiler_instance_expression_node_t *sampler;
    struct compiler_instance_expression_node_t *image;
    struct compiler_instance_expression_list_item_t *first_argument;
};

enum compiler_instance_vector_constructor_variant_t
{
    /// \brief If argument resolves to the same type, constructor is treated as type-enforcing syntax and is skipped.
    COMPILER_INSTANCE_VECTOR_CONSTRUCTOR_SKIP = 0u,

    COMPILER_INSTANCE_VECTOR_CONSTRUCTOR_COMBINE,
    COMPILER_INSTANCE_VECTOR_CONSTRUCTOR_CONVERT,
    COMPILER_INSTANCE_VECTOR_CONSTRUCTOR_FILL,
};

struct compiler_instance_vector_constructor_suffix_t
{
    struct inbuilt_vector_type_t *type;
    enum compiler_instance_vector_constructor_variant_t variant;
    struct compiler_instance_expression_list_item_t *first_argument;
};

enum compiler_instance_matrix_constructor_variant_t
{
    /// \brief If argument resolves to the same type, constructor is treated as type-enforcing syntax and is skipped.
    COMPILER_INSTANCE_MATRIX_CONSTRUCTOR_SKIP = 0u,

    COMPILER_INSTANCE_MATRIX_CONSTRUCTOR_COMBINE,
    COMPILER_INSTANCE_MATRIX_CONSTRUCTOR_CONVERT,
    COMPILER_INSTANCE_MATRIX_CONSTRUCTOR_CROP,
};

struct compiler_instance_matrix_constructor_suffix_t
{
    struct inbuilt_matrix_type_t *type;
    enum compiler_instance_matrix_constructor_variant_t variant;
    struct compiler_instance_expression_list_item_t *first_argument;
};

struct compiler_instance_struct_constructor_suffix_t
{
    struct compiler_instance_struct_node_t *type;
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

struct compiler_instance_expression_node_t
{
    enum compiler_instance_expression_type_t type;
    union
    {
        struct compiler_instance_buffer_node_t *structured_buffer_reference;
        struct compiler_instance_sampler_node_t *sampler_reference;
        struct compiler_instance_image_node_t *image_reference;
        struct compiler_instance_scope_variable_item_t *variable_reference;
        struct compiler_instance_structured_access_suffix_t structured_access;
        struct compiler_instance_swizzle_suffix_t swizzle;
        struct compiler_instance_container_field_node_t *container_field_access;
        float floating_literal;
        kan_rpl_unsigned_int_literal_t unsigned_literal;
        kan_rpl_signed_int_literal_t signed_literal;
        struct compiler_instance_variable_declaration_suffix_t variable_declaration;
        struct compiler_instance_binary_operation_suffix_t binary_operation;
        struct compiler_instance_unary_operation_suffix_t unary_operation;
        struct compiler_instance_scope_suffix_t scope;
        struct compiler_instance_function_call_suffix_t function_call;

        /// \brief Used for both normal and dref sampling.
        struct compiler_instance_image_sample_suffix_t image_sample;

        struct compiler_instance_vector_constructor_suffix_t vector_constructor;
        struct compiler_instance_matrix_constructor_suffix_t matrix_constructor;
        struct compiler_instance_struct_constructor_suffix_t struct_constructor;
        struct compiler_instance_if_suffix_t if_;
        struct compiler_instance_for_suffix_t for_;
        struct compiler_instance_while_suffix_t while_;
        struct compiler_instance_expression_node_t *break_loop;
        struct compiler_instance_expression_node_t *continue_loop;
        struct compiler_instance_expression_node_t *return_expression;
    };

    struct compiler_instance_type_definition_t output;
    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    kan_rpl_size_t source_line;
};

struct compiler_instance_container_access_node_t
{
    struct compiler_instance_container_access_node_t *next;
    struct compiler_instance_container_node_t *container;
    struct compiler_instance_function_node_t *direct_access_function;
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

struct compiler_instance_image_access_node_t
{
    struct compiler_instance_image_access_node_t *next;
    struct compiler_instance_image_node_t *image;
    struct compiler_instance_function_node_t *direct_access_function;
};

struct compiler_instance_function_argument_node_t
{
    struct compiler_instance_function_argument_node_t *next;
    struct compiler_instance_variable_t variable;

    kan_interned_string_t module_name;
    kan_interned_string_t source_name;
    kan_rpl_size_t source_line;
};

struct compiler_instance_function_node_t
{
    struct compiler_instance_function_node_t *next;
    kan_interned_string_t name;
    struct compiler_instance_type_definition_t return_type;

    struct compiler_instance_function_argument_node_t *first_argument;
    struct compiler_instance_expression_node_t *body;
    struct compiler_instance_scope_variable_item_t *first_argument_variable;

    kan_bool_t has_stage_specific_access;
    enum kan_rpl_pipeline_stage_t required_stage;
    struct compiler_instance_container_access_node_t *first_container_access;
    struct compiler_instance_buffer_access_node_t *first_buffer_access;
    struct compiler_instance_sampler_access_node_t *first_sampler_access;
    struct compiler_instance_image_access_node_t *first_image_access;

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

    struct compiler_instance_container_node_t *first_container;
    struct compiler_instance_container_node_t *last_container;

    struct compiler_instance_buffer_node_t *first_buffer;
    struct compiler_instance_buffer_node_t *last_buffer;

    struct compiler_instance_sampler_node_t *first_sampler;
    struct compiler_instance_sampler_node_t *last_sampler;

    struct compiler_instance_image_node_t *first_image;
    struct compiler_instance_image_node_t *last_image;

    struct compiler_instance_function_node_t *first_function;
    struct compiler_instance_function_node_t *last_function;
};

enum inbuilt_type_item_t
{
    INBUILT_TYPE_ITEM_FLOAT = 0u,
    INBUILT_TYPE_ITEM_UNSIGNED,
    INBUILT_TYPE_ITEM_SIGNED,
};

static inline kan_bool_t inbuilt_type_item_is_integer (enum inbuilt_type_item_t item)
{
    switch (item)
    {
    case INBUILT_TYPE_ITEM_FLOAT:
        return KAN_FALSE;

    case INBUILT_TYPE_ITEM_UNSIGNED:
    case INBUILT_TYPE_ITEM_SIGNED:
        return KAN_TRUE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static kan_instance_size_t inbuilt_type_item_size[] = {
    4u,
    4u,
    4u,
};

#define INBUILT_ITEM_TYPE_COUNT (sizeof (inbuilt_type_item_size) / sizeof (inbuilt_type_item_size[0u]))
#define INBUILT_VECTOR_MAX_ITEMS 4u
#define INBUILT_VECTOR_TYPE_INDEX(ITEM_TYPE, ITEM_COUNT)                                                               \
    ((kan_instance_size_t) (ITEM_TYPE)) * INBUILT_VECTOR_MAX_ITEMS + (ITEM_COUNT) - 1u
#define INBUILT_VECTOR_TYPE_COUNT INBUILT_ITEM_TYPE_COUNT *INBUILT_VECTOR_MAX_ITEMS

struct inbuilt_vector_type_t
{
    kan_interned_string_t name;
    enum inbuilt_type_item_t item;
    kan_instance_size_t items_count;
    enum kan_rpl_meta_variable_type_t meta_type;
};

#define INBUILT_MATRIX_MAX_COLUMNS 4u
#define INBUILT_MATRIX_MAX_ROWS INBUILT_VECTOR_MAX_ITEMS

struct inbuilt_matrix_type_t
{
    kan_interned_string_t name;
    enum inbuilt_type_item_t item;
    kan_instance_size_t rows;
    kan_instance_size_t columns;
    enum kan_rpl_meta_variable_type_t meta_type;
};

/// \details In common header as they are needed to initialize statics with known fixed ids.
enum spirv_fixed_ids_t
{
    SPIRV_FIXED_ID_INVALID = 0u,

    SPIRV_FIXED_ID_TYPE_VOID = 1u,
    SPIRV_FIXED_ID_TYPE_BOOLEAN,

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
    kan_interned_string_t interned_sampler;

    kan_interned_string_t interned_image_color_2d;
    kan_interned_string_t interned_image_color_3d;
    kan_interned_string_t interned_image_color_cube;
    kan_interned_string_t interned_image_color_2d_array;
    kan_interned_string_t interned_image_depth_2d;
    kan_interned_string_t interned_image_depth_3d;
    kan_interned_string_t interned_image_depth_cube;
    kan_interned_string_t interned_image_depth_2d_array;

    kan_interned_string_t interned_in;
    kan_interned_string_t interned_out;

    struct inbuilt_vector_type_t vector_types[INBUILT_VECTOR_TYPE_COUNT];
#define INBUILT_MATRIX_TYPE_COUNT 2u
    struct inbuilt_matrix_type_t matrix_types[INBUILT_MATRIX_TYPE_COUNT];

    kan_interned_string_t sample_function_name;
    kan_interned_string_t sample_dref_function_name;

    struct compiler_instance_function_argument_node_t sample_2d_additional_arguments[1u];
    struct compiler_instance_function_argument_node_t sample_3d_additional_arguments[1u];
    struct compiler_instance_function_argument_node_t sample_cube_additional_arguments[1u];
    struct compiler_instance_function_argument_node_t sample_2d_array_additional_arguments[2u];

    struct compiler_instance_function_argument_node_t sample_dref_2d_additional_arguments[2u];
    struct compiler_instance_function_argument_node_t sample_dref_3d_additional_arguments[2u];
    struct compiler_instance_function_argument_node_t sample_dref_cube_additional_arguments[2u];
    struct compiler_instance_function_argument_node_t sample_dref_2d_array_additional_arguments[3u];

    struct compiler_instance_declaration_node_t *sampler_2d_call_signature_first_element;
    struct compiler_instance_declaration_node_t sampler_2d_call_signature_location;

    struct kan_hash_storage_t builtin_hash_storage;

#define BUILTIN_FUNCTION_FIELD(NAME, ARGUMENTS_COUNT)                                                                  \
    struct compiler_instance_function_node_t builtin_##NAME;                                                           \
    struct compiler_instance_function_argument_node_t builtin_##NAME##_arguments[ARGUMENTS_COUNT]

    BUILTIN_FUNCTION_FIELD (vertex_stage_output_position, 1u);

    struct compiler_instance_function_node_t builtin_pi;

    BUILTIN_FUNCTION_FIELD (round_f1, 1u);
    BUILTIN_FUNCTION_FIELD (round_f2, 1u);
    BUILTIN_FUNCTION_FIELD (round_f3, 1u);
    BUILTIN_FUNCTION_FIELD (round_f4, 1u);
    BUILTIN_FUNCTION_FIELD (round_even_f1, 1u);
    BUILTIN_FUNCTION_FIELD (round_even_f2, 1u);
    BUILTIN_FUNCTION_FIELD (round_even_f3, 1u);
    BUILTIN_FUNCTION_FIELD (round_even_f4, 1u);
    BUILTIN_FUNCTION_FIELD (trunc_f1, 1u);
    BUILTIN_FUNCTION_FIELD (trunc_f2, 1u);
    BUILTIN_FUNCTION_FIELD (trunc_f3, 1u);
    BUILTIN_FUNCTION_FIELD (trunc_f4, 1u);
    BUILTIN_FUNCTION_FIELD (abs_f1, 1u);
    BUILTIN_FUNCTION_FIELD (abs_f2, 1u);
    BUILTIN_FUNCTION_FIELD (abs_f3, 1u);
    BUILTIN_FUNCTION_FIELD (abs_f4, 1u);
    BUILTIN_FUNCTION_FIELD (abs_s1, 1u);
    BUILTIN_FUNCTION_FIELD (abs_s2, 1u);
    BUILTIN_FUNCTION_FIELD (abs_s3, 1u);
    BUILTIN_FUNCTION_FIELD (abs_s4, 1u);
    BUILTIN_FUNCTION_FIELD (sign_f1, 1u);
    BUILTIN_FUNCTION_FIELD (sign_f2, 1u);
    BUILTIN_FUNCTION_FIELD (sign_f3, 1u);
    BUILTIN_FUNCTION_FIELD (sign_f4, 1u);
    BUILTIN_FUNCTION_FIELD (sign_s1, 1u);
    BUILTIN_FUNCTION_FIELD (sign_s2, 1u);
    BUILTIN_FUNCTION_FIELD (sign_s3, 1u);
    BUILTIN_FUNCTION_FIELD (sign_s4, 1u);
    BUILTIN_FUNCTION_FIELD (floor_f1, 1u);
    BUILTIN_FUNCTION_FIELD (floor_f2, 1u);
    BUILTIN_FUNCTION_FIELD (floor_f3, 1u);
    BUILTIN_FUNCTION_FIELD (floor_f4, 1u);
    BUILTIN_FUNCTION_FIELD (ceil_f1, 1u);
    BUILTIN_FUNCTION_FIELD (ceil_f2, 1u);
    BUILTIN_FUNCTION_FIELD (ceil_f3, 1u);
    BUILTIN_FUNCTION_FIELD (ceil_f4, 1u);
    BUILTIN_FUNCTION_FIELD (fract_f1, 1u);
    BUILTIN_FUNCTION_FIELD (fract_f2, 1u);
    BUILTIN_FUNCTION_FIELD (fract_f3, 1u);
    BUILTIN_FUNCTION_FIELD (fract_f4, 1u);
    BUILTIN_FUNCTION_FIELD (sin_f1, 1u);
    BUILTIN_FUNCTION_FIELD (sin_f2, 1u);
    BUILTIN_FUNCTION_FIELD (sin_f3, 1u);
    BUILTIN_FUNCTION_FIELD (sin_f4, 1u);
    BUILTIN_FUNCTION_FIELD (cos_f1, 1u);
    BUILTIN_FUNCTION_FIELD (cos_f2, 1u);
    BUILTIN_FUNCTION_FIELD (cos_f3, 1u);
    BUILTIN_FUNCTION_FIELD (cos_f4, 1u);
    BUILTIN_FUNCTION_FIELD (tan_f1, 1u);
    BUILTIN_FUNCTION_FIELD (tan_f2, 1u);
    BUILTIN_FUNCTION_FIELD (tan_f3, 1u);
    BUILTIN_FUNCTION_FIELD (tan_f4, 1u);
    BUILTIN_FUNCTION_FIELD (asin_f1, 1u);
    BUILTIN_FUNCTION_FIELD (asin_f2, 1u);
    BUILTIN_FUNCTION_FIELD (asin_f3, 1u);
    BUILTIN_FUNCTION_FIELD (asin_f4, 1u);
    BUILTIN_FUNCTION_FIELD (acos_f1, 1u);
    BUILTIN_FUNCTION_FIELD (acos_f2, 1u);
    BUILTIN_FUNCTION_FIELD (acos_f3, 1u);
    BUILTIN_FUNCTION_FIELD (acos_f4, 1u);
    BUILTIN_FUNCTION_FIELD (atan_f1, 1u);
    BUILTIN_FUNCTION_FIELD (atan_f2, 1u);
    BUILTIN_FUNCTION_FIELD (atan_f3, 1u);
    BUILTIN_FUNCTION_FIELD (atan_f4, 1u);
    BUILTIN_FUNCTION_FIELD (sinh_f1, 1u);
    BUILTIN_FUNCTION_FIELD (sinh_f2, 1u);
    BUILTIN_FUNCTION_FIELD (sinh_f3, 1u);
    BUILTIN_FUNCTION_FIELD (sinh_f4, 1u);
    BUILTIN_FUNCTION_FIELD (cosh_f1, 1u);
    BUILTIN_FUNCTION_FIELD (cosh_f2, 1u);
    BUILTIN_FUNCTION_FIELD (cosh_f3, 1u);
    BUILTIN_FUNCTION_FIELD (cosh_f4, 1u);
    BUILTIN_FUNCTION_FIELD (tanh_f1, 1u);
    BUILTIN_FUNCTION_FIELD (tanh_f2, 1u);
    BUILTIN_FUNCTION_FIELD (tanh_f3, 1u);
    BUILTIN_FUNCTION_FIELD (tanh_f4, 1u);
    BUILTIN_FUNCTION_FIELD (asinh_f1, 1u);
    BUILTIN_FUNCTION_FIELD (asinh_f2, 1u);
    BUILTIN_FUNCTION_FIELD (asinh_f3, 1u);
    BUILTIN_FUNCTION_FIELD (asinh_f4, 1u);
    BUILTIN_FUNCTION_FIELD (acosh_f1, 1u);
    BUILTIN_FUNCTION_FIELD (acosh_f2, 1u);
    BUILTIN_FUNCTION_FIELD (acosh_f3, 1u);
    BUILTIN_FUNCTION_FIELD (acosh_f4, 1u);
    BUILTIN_FUNCTION_FIELD (atanh_f1, 1u);
    BUILTIN_FUNCTION_FIELD (atanh_f2, 1u);
    BUILTIN_FUNCTION_FIELD (atanh_f3, 1u);
    BUILTIN_FUNCTION_FIELD (atanh_f4, 1u);
    BUILTIN_FUNCTION_FIELD (atan2_f1, 1u);
    BUILTIN_FUNCTION_FIELD (atan2_f2, 1u);
    BUILTIN_FUNCTION_FIELD (atan2_f3, 1u);
    BUILTIN_FUNCTION_FIELD (atan2_f4, 1u);
    BUILTIN_FUNCTION_FIELD (pow_f1, 2u);
    BUILTIN_FUNCTION_FIELD (pow_f2, 2u);
    BUILTIN_FUNCTION_FIELD (pow_f3, 2u);
    BUILTIN_FUNCTION_FIELD (pow_f4, 2u);
    BUILTIN_FUNCTION_FIELD (exp_f1, 1u);
    BUILTIN_FUNCTION_FIELD (exp_f2, 1u);
    BUILTIN_FUNCTION_FIELD (exp_f3, 1u);
    BUILTIN_FUNCTION_FIELD (exp_f4, 1u);
    BUILTIN_FUNCTION_FIELD (log_f1, 1u);
    BUILTIN_FUNCTION_FIELD (log_f2, 1u);
    BUILTIN_FUNCTION_FIELD (log_f3, 1u);
    BUILTIN_FUNCTION_FIELD (log_f4, 1u);
    BUILTIN_FUNCTION_FIELD (exp2_f1, 1u);
    BUILTIN_FUNCTION_FIELD (exp2_f2, 1u);
    BUILTIN_FUNCTION_FIELD (exp2_f3, 1u);
    BUILTIN_FUNCTION_FIELD (exp2_f4, 1u);
    BUILTIN_FUNCTION_FIELD (log2_f1, 1u);
    BUILTIN_FUNCTION_FIELD (log2_f2, 1u);
    BUILTIN_FUNCTION_FIELD (log2_f3, 1u);
    BUILTIN_FUNCTION_FIELD (log2_f4, 1u);
    BUILTIN_FUNCTION_FIELD (sqrt_f1, 1u);
    BUILTIN_FUNCTION_FIELD (sqrt_f2, 1u);
    BUILTIN_FUNCTION_FIELD (sqrt_f3, 1u);
    BUILTIN_FUNCTION_FIELD (sqrt_f4, 1u);
    BUILTIN_FUNCTION_FIELD (inverse_sqrt_f1, 1u);
    BUILTIN_FUNCTION_FIELD (inverse_sqrt_f2, 1u);
    BUILTIN_FUNCTION_FIELD (inverse_sqrt_f3, 1u);
    BUILTIN_FUNCTION_FIELD (inverse_sqrt_f4, 1u);
    BUILTIN_FUNCTION_FIELD (determinant_f3x3, 1u);
    BUILTIN_FUNCTION_FIELD (determinant_f4x4, 1u);
    BUILTIN_FUNCTION_FIELD (inverse_f3x3, 1u);
    BUILTIN_FUNCTION_FIELD (inverse_f4x4, 1u);
    BUILTIN_FUNCTION_FIELD (transpose_matrix_f3x3, 1u);
    BUILTIN_FUNCTION_FIELD (transpose_matrix_f4x4, 1u);
    BUILTIN_FUNCTION_FIELD (min_f1, 2u);
    BUILTIN_FUNCTION_FIELD (min_f2, 2u);
    BUILTIN_FUNCTION_FIELD (min_f3, 2u);
    BUILTIN_FUNCTION_FIELD (min_f4, 2u);
    BUILTIN_FUNCTION_FIELD (min_u1, 2u);
    BUILTIN_FUNCTION_FIELD (min_u2, 2u);
    BUILTIN_FUNCTION_FIELD (min_u3, 2u);
    BUILTIN_FUNCTION_FIELD (min_u4, 2u);
    BUILTIN_FUNCTION_FIELD (min_s1, 2u);
    BUILTIN_FUNCTION_FIELD (min_s2, 2u);
    BUILTIN_FUNCTION_FIELD (min_s3, 2u);
    BUILTIN_FUNCTION_FIELD (min_s4, 2u);
    BUILTIN_FUNCTION_FIELD (max_f1, 2u);
    BUILTIN_FUNCTION_FIELD (max_f2, 2u);
    BUILTIN_FUNCTION_FIELD (max_f3, 2u);
    BUILTIN_FUNCTION_FIELD (max_f4, 2u);
    BUILTIN_FUNCTION_FIELD (max_u1, 2u);
    BUILTIN_FUNCTION_FIELD (max_u2, 2u);
    BUILTIN_FUNCTION_FIELD (max_u3, 2u);
    BUILTIN_FUNCTION_FIELD (max_u4, 2u);
    BUILTIN_FUNCTION_FIELD (max_s1, 2u);
    BUILTIN_FUNCTION_FIELD (max_s2, 2u);
    BUILTIN_FUNCTION_FIELD (max_s3, 2u);
    BUILTIN_FUNCTION_FIELD (max_s4, 2u);
    BUILTIN_FUNCTION_FIELD (clamp_f1, 3u);
    BUILTIN_FUNCTION_FIELD (clamp_f2, 3u);
    BUILTIN_FUNCTION_FIELD (clamp_f3, 3u);
    BUILTIN_FUNCTION_FIELD (clamp_f4, 3u);
    BUILTIN_FUNCTION_FIELD (clamp_u1, 3u);
    BUILTIN_FUNCTION_FIELD (clamp_u2, 3u);
    BUILTIN_FUNCTION_FIELD (clamp_u3, 3u);
    BUILTIN_FUNCTION_FIELD (clamp_u4, 3u);
    BUILTIN_FUNCTION_FIELD (clamp_s1, 3u);
    BUILTIN_FUNCTION_FIELD (clamp_s2, 3u);
    BUILTIN_FUNCTION_FIELD (clamp_s3, 3u);
    BUILTIN_FUNCTION_FIELD (clamp_s4, 3u);
    BUILTIN_FUNCTION_FIELD (mix_f1, 3u);
    BUILTIN_FUNCTION_FIELD (mix_f2, 3u);
    BUILTIN_FUNCTION_FIELD (mix_f3, 3u);
    BUILTIN_FUNCTION_FIELD (mix_f4, 3u);
    BUILTIN_FUNCTION_FIELD (step_f1, 2u);
    BUILTIN_FUNCTION_FIELD (step_f2, 2u);
    BUILTIN_FUNCTION_FIELD (step_f3, 2u);
    BUILTIN_FUNCTION_FIELD (step_f4, 2u);
    BUILTIN_FUNCTION_FIELD (fma_f1, 3u);
    BUILTIN_FUNCTION_FIELD (fma_f2, 3u);
    BUILTIN_FUNCTION_FIELD (fma_f3, 3u);
    BUILTIN_FUNCTION_FIELD (fma_f4, 3u);
    BUILTIN_FUNCTION_FIELD (length_f2, 1u);
    BUILTIN_FUNCTION_FIELD (length_f3, 1u);
    BUILTIN_FUNCTION_FIELD (length_f4, 1u);
    BUILTIN_FUNCTION_FIELD (distance_f2, 2u);
    BUILTIN_FUNCTION_FIELD (distance_f3, 2u);
    BUILTIN_FUNCTION_FIELD (distance_f4, 2u);
    BUILTIN_FUNCTION_FIELD (cross_f3, 2u);
    BUILTIN_FUNCTION_FIELD (dot_f2, 2u);
    BUILTIN_FUNCTION_FIELD (dot_f3, 2u);
    BUILTIN_FUNCTION_FIELD (dot_f4, 2u);
    BUILTIN_FUNCTION_FIELD (normalize_f2, 1u);
    BUILTIN_FUNCTION_FIELD (normalize_f3, 1u);
    BUILTIN_FUNCTION_FIELD (normalize_f4, 1u);
    BUILTIN_FUNCTION_FIELD (reflect_f1, 2u);
    BUILTIN_FUNCTION_FIELD (reflect_f2, 2u);
    BUILTIN_FUNCTION_FIELD (reflect_f3, 2u);
    BUILTIN_FUNCTION_FIELD (reflect_f4, 2u);
    BUILTIN_FUNCTION_FIELD (refract_f1, 3u);
    BUILTIN_FUNCTION_FIELD (refract_f2, 3u);
    BUILTIN_FUNCTION_FIELD (refract_f3, 3u);
    BUILTIN_FUNCTION_FIELD (refract_f4, 3u);
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
        if (kan_rpl_compiler_statics.vector_types[index].name == name)
        {
            return &kan_rpl_compiler_statics.vector_types[index];
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
        if (kan_rpl_compiler_statics.matrix_types[index].name == name)
        {
            return &kan_rpl_compiler_statics.matrix_types[index];
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

static inline const char *get_image_type_name (enum kan_rpl_image_type_t type)
{
    switch (type)
    {
    case KAN_RPL_IMAGE_TYPE_COLOR_2D:
        return "image_color_2d";

    case KAN_RPL_IMAGE_TYPE_COLOR_3D:
        return "image_color_3d";

    case KAN_RPL_IMAGE_TYPE_COLOR_CUBE:
        return "image_color_cube";

    case KAN_RPL_IMAGE_TYPE_COLOR_2D_ARRAY:
        return "image_color_2d_array";

    case KAN_RPL_IMAGE_TYPE_DEPTH_2D:
        return "image_depth_2d";

    case KAN_RPL_IMAGE_TYPE_DEPTH_3D:
        return "image_depth_3d";

    case KAN_RPL_IMAGE_TYPE_DEPTH_CUBE:
        return "image_depth_cube";

    case KAN_RPL_IMAGE_TYPE_DEPTH_2D_ARRAY:
        return "image_depth_2d_array";

    case KAN_RPL_IMAGE_TYPE_COUNT:
        KAN_ASSERT (KAN_FALSE)
        break;
    }

    return "<unknown_image_type>";
}

static inline const char *get_type_name_for_logging (struct compiler_instance_type_definition_t *definition)
{
    switch (definition->class)
    {
    case COMPILER_INSTANCE_TYPE_CLASS_VOID:
        return "void";

    case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
        return definition->vector_data->name;

    case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
        return definition->matrix_data->name;

    case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
        return definition->struct_data->name;

    case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
        return definition->buffer_data->name;

    case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
        return "boolean";

    case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
        return "sampler";

    case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
        return get_image_type_name (definition->image_type);
    }

    return "<unknown_type>";
}

static inline void calculate_type_definition_size_and_alignment (struct compiler_instance_type_definition_t *definition,
                                                                 kan_instance_size_t dimension_offset,
                                                                 kan_instance_size_t *size,
                                                                 kan_instance_size_t *alignment)
{
    *size = 0u;
    *alignment = 0u;

    switch (definition->class)
    {
    case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
        *size = inbuilt_type_item_size[definition->vector_data->item] * definition->vector_data->items_count;
        *alignment = inbuilt_type_item_size[definition->vector_data->item];
        break;

    case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
        *size = inbuilt_type_item_size[definition->matrix_data->item] * definition->matrix_data->rows *
                definition->matrix_data->columns;
        *alignment = inbuilt_type_item_size[definition->matrix_data->item];
        break;

    case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
        *size = definition->struct_data->size;
        *alignment = definition->struct_data->alignment;
        break;

    case COMPILER_INSTANCE_TYPE_CLASS_VOID:
    case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
    case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
    case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
    case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
        // Shouldn't get there. Callers should validate that size and alignment is not calculated for opaque type.
        KAN_ASSERT (KAN_FALSE)
        break;
    }

    for (kan_loop_size_t dimension = dimension_offset; dimension < definition->array_dimensions_count; ++dimension)
    {
        *size *= definition->array_dimensions[dimension];
    }
}

/// \brief Checks if type definition base types (array specifiers, access and flags are excluded) are equal.
static inline kan_bool_t is_type_definition_base_equal (struct compiler_instance_type_definition_t *left,
                                                        struct compiler_instance_type_definition_t *right)
{
    if (left->class != right->class)
    {
        return KAN_FALSE;
    }

    switch (left->class)
    {
    case COMPILER_INSTANCE_TYPE_CLASS_VOID:
    case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
    case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
        return KAN_TRUE;

    case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
        return left->vector_data == right->vector_data;

    case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
        return left->matrix_data == right->matrix_data;

    case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
        return left->buffer_data == right->buffer_data;

    case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
        return left->struct_data == right->struct_data;

    case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
        return left->image_type == right->image_type;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline void copy_type_definition (struct compiler_instance_type_definition_t *output,
                                         const struct compiler_instance_type_definition_t *source)
{
    output->class = source->class;
    switch (output->class)
    {
    case COMPILER_INSTANCE_TYPE_CLASS_VOID:
    case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
    case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
        break;

    case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
        output->vector_data = source->vector_data;
        break;

    case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
        output->matrix_data = source->matrix_data;
        break;

    case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
        output->struct_data = source->struct_data;
        break;

    case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
        output->buffer_data = source->buffer_data;
        break;

    case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
        output->image_type = source->image_type;
        break;
    }

    output->access = source->access;
    output->flags = source->flags;

    output->array_size_runtime = source->array_size_runtime;
    output->array_dimensions_count = source->array_dimensions_count;
    output->array_dimensions = source->array_dimensions;
}
