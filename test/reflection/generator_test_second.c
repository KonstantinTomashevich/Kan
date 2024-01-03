#include <generator_test_second.h>

void first_component_init (kan_reflection_functor_user_data_t user_data, void *generic_instance)
{
    *(struct first_component_t *) generic_instance = (struct first_component_t) {
        .position = {.x = 0.0f, .y = 0.0f, .z = 0.0f},
        .rotation = {.x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f},
    };
}

void first_component_shutdown (kan_reflection_functor_user_data_t user_data, void *generic_instance)
{
}

void second_component_init (kan_reflection_functor_user_data_t user_data, void *generic_instance)
{
    *(struct second_component_t *) generic_instance = (struct second_component_t) {
        .velocity = {.x = 0.0f, .y = 0.0f, .z = 0.0f},
        .acceleration = {.x = 0.0f, .y = 0.0f, .z = 0.0f},
    };
}

void second_component_shutdown (kan_reflection_functor_user_data_t user_data, void *generic_instance)
{
}
