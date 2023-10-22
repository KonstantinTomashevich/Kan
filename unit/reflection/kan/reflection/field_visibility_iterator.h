#pragma once

#include <reflection_api.h>

#include <kan/api_common/c_header.h>
#include <kan/error/critical.h>
#include <kan/reflection/registry.h>

/// \file
/// \brief Provides inlined utilities for iterating over structure fields using visibility rules.
///
/// \par Visibility-driven iteration
/// \parblock
/// Visibility options can be attached to every field of a structure and they can be checked if we have actual structure
/// instance. It is useful for tasks like serialization where we need to skip fields that aren't visible in instance
/// context, for example inactive union fields.
///
/// To iterate over struct fields while checking visibility rules, you need to create and initialize iterator:
/// ```c
/// struct kan_reflection_visibility_iterator_t iterator;
/// kan_reflection_visibility_iterator_init (&iterator, type_of_your_struct, pointer_to_your_struct);
/// ```
///
/// Then you can move iterator using `kan_reflection_visibility_iterator_advance` until `field` field is equal to
/// `field_end` field, where field end points to the first position of memory that doesn't actually contain any field.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Contains visibility-based iterator context.
struct kan_reflection_visibility_iterator_t
{
    struct kan_reflection_field_t *field;
    struct kan_reflection_field_t *field_end;
    const void *context;
};

/// \brief Initializes visibility-based field iterator.
static inline void kan_reflection_visibility_iterator_init (struct kan_reflection_visibility_iterator_t *iterator,
                                                            const struct kan_reflection_struct_t *struct_reflection,
                                                            const void *context)
{
    KAN_ASSERT (struct_reflection->fields)
    KAN_ASSERT (struct_reflection->fields_count > 0u)

    iterator->field = struct_reflection->fields;
    iterator->field_end = struct_reflection->fields + struct_reflection->fields_count;
    iterator->context = context;
}

/// \brief Advances visibility-based iterator to the next visible field.
static inline void kan_reflection_visibility_iterator_advance (struct kan_reflection_visibility_iterator_t *iterator)
{
    if (iterator->field == iterator->field_end)
    {
        return;
    }

    ++iterator->field;
    while (iterator->field != iterator->field_end)
    {
        if (iterator->field->visibility_condition_values_count > 0u)
        {
            KAN_ASSERT (iterator->field->visibility_condition_field)

#define CHECK_CONDITIONS(TYPE)                                                                                         \
    for (uint64_t index = 0u; index < iterator->field->visibility_condition_values_count; ++index)                     \
    {                                                                                                                  \
        if (*(const TYPE *) (((const uint8_t *) iterator->context) +                                                   \
                             iterator->field->visibility_condition_field->offset) ==                                   \
            (TYPE) iterator->field->visibility_condition_values[index])                                                \
        {                                                                                                              \
            return;                                                                                                    \
        }                                                                                                              \
    }

            switch (iterator->field->visibility_condition_field->archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
                switch (iterator->field->visibility_condition_field->size)
                {
                case 1u:
                    CHECK_CONDITIONS (int8_t)
                    break;
                case 2u:
                    CHECK_CONDITIONS (int16_t)
                    break;
                case 4u:
                    CHECK_CONDITIONS (int32_t)
                    break;
                case 8u:
                    CHECK_CONDITIONS (int64_t)
                    break;
                default:
                    KAN_ASSERT (KAN_FALSE)
                    break;
                }

                break;

            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
                switch (iterator->field->visibility_condition_field->size)
                {
                case 1u:
                    CHECK_CONDITIONS (uint8_t)
                    break;
                case 2u:
                    CHECK_CONDITIONS (uint16_t)
                    break;
                case 4u:
                    CHECK_CONDITIONS (uint32_t)
                    break;
                case 8u:
                    CHECK_CONDITIONS (uint64_t)
                    break;
                default:
                    KAN_ASSERT (KAN_FALSE)
                    break;
                }

                break;

            case KAN_REFLECTION_ARCHETYPE_ENUM:
                switch (iterator->field->visibility_condition_field->size)
                {
                case sizeof (int):
                    CHECK_CONDITIONS (int)
                    break;
                default:
                    KAN_ASSERT (KAN_FALSE)
                    break;
                }

                break;

            case KAN_REFLECTION_ARCHETYPE_FLOATING:
            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            case KAN_REFLECTION_ARCHETYPE_STRUCT:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_PATCH:
                KAN_ASSERT (KAN_FALSE)
                break;
            }

#undef CHECK_CONDITIONS
        }

        else
        {
            return;
        }

        ++iterator->field;
    }
}

KAN_C_HEADER_END
