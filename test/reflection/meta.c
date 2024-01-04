#include <stdint.h>

#include <test_reflection_api.h>

#include <generator_test_first.h>

struct network_meta_t
{
    struct vector3_t min;
    struct vector3_t max;
    uint8_t bits;
};

// \meta reflection_struct_field_meta = "first_component_t.position"
TEST_REFLECTION_API struct network_meta_t first_component_position_network_meta = {
    .min = {.x = -100.0f, .y = -10.0f, .z = -100.0f},
    .max = {.x = 100.0f, .y = 10.0f, .z = 100.0f},
    .bits = 8,
};

// \meta reflection_struct_field_meta = "second_component_t.velocity"
TEST_REFLECTION_API struct network_meta_t second_component_velocity_network_meta = {
    .min = {.x = -10.0f, .y = -10.0f, .z = -10.0f},
    .max = {.x = 10.0f, .y = 10.0f, .z = 10.0f},
    .bits = 8,
};

struct function_script_graph_meta_t
{
    const char *name;
    const char *hint;
};

// \meta reflection_function_meta = "vector3_add"
TEST_REFLECTION_API struct function_script_graph_meta_t vector3_add_meta = {
    .name = "sum (vector3)",
    .hint = "Produces sum of two 3d vectors",
};

// \meta reflection_function_meta = "vector4_add"
TEST_REFLECTION_API struct function_script_graph_meta_t vector4_add_meta = {
    .name = "sum (vector4)",
    .hint = "Produces sum of two 4d vectors",
};
