#pragma once

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

/// \brief Utility method for checking visibility conditions.
/// \param visibility_condition_field Information about field that holds visibility-deciding value.
/// \param visibility_condition_values_count Number of elements in `visibility_condition_values` array.
/// \param visibility_condition_values Array of values that trigger visibility condition.
/// \param visibility_value_pointer_with_offset Pointer to visibility value to be used. We don't request full context
///                                             and manually offset it, because it is easier for other reflection code.
static inline bool kan_reflection_check_visibility (const struct kan_reflection_field_t *visibility_condition_field,
                                                    kan_instance_size_t visibility_condition_values_count,
                                                    const kan_instance_offset_t *visibility_condition_values,
                                                    const void *visibility_value_pointer_with_offset)
{
    if (visibility_condition_values_count > 0u)
    {
        KAN_ASSERT (visibility_condition_field)

#define CHECK_CONDITIONS(TYPE)                                                                                         \
    for (kan_loop_size_t index = 0u; index < visibility_condition_values_count; ++index)                               \
    {                                                                                                                  \
        if (*(const TYPE *) (visibility_value_pointer_with_offset) == (TYPE) visibility_condition_values[index])       \
        {                                                                                                              \
            return true;                                                                                               \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    return false;

        switch (visibility_condition_field->archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
            switch (visibility_condition_field->size)
            {
            case 1u:
                CHECK_CONDITIONS (int8_t)
            case 2u:
                CHECK_CONDITIONS (int16_t)
            case 4u:
                CHECK_CONDITIONS (int32_t)
            case 8u:
                CHECK_CONDITIONS (int64_t)
            default:
                KAN_ASSERT (false)
                break;
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
            switch (visibility_condition_field->size)
            {
            case 1u:
                CHECK_CONDITIONS (uint8_t)
            case 2u:
                CHECK_CONDITIONS (uint16_t)
            case 4u:
                CHECK_CONDITIONS (uint32_t)
            case 8u:
                CHECK_CONDITIONS (uint64_t)
            default:
                KAN_ASSERT (false)
                break;
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        case KAN_REFLECTION_ARCHETYPE_STRUCT:
        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_PATCH:
            KAN_ASSERT (false)
            break;
        }

#undef CHECK_CONDITIONS
        return false;
    }

    return true;
}

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
        if (kan_reflection_check_visibility (
                iterator->field->visibility_condition_field, iterator->field->visibility_condition_values_count,
                iterator->field->visibility_condition_values,
                ((const uint8_t *) iterator->context) + (iterator->field->visibility_condition_field ?
                                                             iterator->field->visibility_condition_field->offset :
                                                             0u)))
        {
            return;
        }

        ++iterator->field;
    }
}

KAN_C_HEADER_END
