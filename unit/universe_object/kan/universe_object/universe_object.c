#include <kan/universe_object/universe_object.h>

void kan_object_id_generator_singleton_init (struct kan_object_id_generator_singleton_t *instance)
{
    instance->counter = kan_atomic_int_init (1);
}
