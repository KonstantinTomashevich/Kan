#include <generator_test_first.h>

struct vector3_t vector3_add (const struct vector3_t *first, const struct vector3_t *second)
{
    struct vector3_t result;
    result.x = first->x + second->x;
    result.y = first->y + second->y;
    result.z = first->z + second->z;
    return result;
}

struct vector4_t vector4_add (const struct vector4_t *first, const struct vector4_t *second)
{
    struct vector4_t result;
    result.x = first->x + second->x;
    result.y = first->y + second->y;
    result.z = first->z + second->z;
    result.w = first->w + second->w;
    return result;
}
