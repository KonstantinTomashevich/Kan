#include <generator_test_second.h>

void first_component_init (struct first_component_t *instance)
{
    instance->position = (struct vector3_t) {.x = 0.0f, .y = 0.0f, .z = 0.0f};
    instance->rotation = (struct vector4_t) {.x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f};
}

void first_component_shutdown (struct first_component_t *instance) {}

void second_component_init (struct second_component_t *instance)
{
    instance->velocity = (struct vector3_t) {.x = 0.0f, .y = 0.0f, .z = 0.0f};
    instance->acceleration = (struct vector3_t) {.x = 0.0f, .y = 0.0f, .z = 0.0f};
}

void second_component_shutdown (struct second_component_t *instance) {}
