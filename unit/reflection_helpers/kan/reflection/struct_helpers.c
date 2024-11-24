#include <string.h>

#include <kan/container/dynamic_array.h>
#include <kan/error/critical.h>
#include <kan/reflection/field_visibility_iterator.h>
#include <kan/reflection/patch.h>
#include <kan/reflection/struct_helpers.h>

kan_instance_size_t kan_reflection_get_inline_array_size (const struct kan_reflection_field_t *array_field,
                                                          const void *owner_struct_instance)
{
    if (array_field->archetype_inline_array.size_field)
    {
        const struct kan_reflection_field_t *size_field = array_field->archetype_inline_array.size_field;
        const void *size_field_address = ((const uint8_t *) owner_struct_instance) + size_field->offset;

        switch (size_field->archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            switch (size_field->size)
            {
            case 1u:
                return (kan_instance_size_t) * (int8_t *) size_field_address;
            case 2u:
                return (kan_instance_size_t) * (int16_t *) size_field_address;
            case 4u:
                return (kan_instance_size_t) * (int32_t *) size_field_address;
            case 8u:
                return (kan_instance_size_t) * (int64_t *) size_field_address;
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            switch (size_field->size)
            {
            case 1u:
                return (kan_instance_size_t) * (uint8_t *) size_field_address;
            case 2u:
                return (kan_instance_size_t) * (uint16_t *) size_field_address;
            case 4u:
                return (kan_instance_size_t) * (uint32_t *) size_field_address;
            case 8u:
                return (kan_instance_size_t) * (uint64_t *) size_field_address;
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        case KAN_REFLECTION_ARCHETYPE_STRUCT:
        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_PATCH:
            KAN_ASSERT (KAN_FALSE)
            break;
        }
    }

    return array_field->archetype_inline_array.item_count;
}

static void kan_reflection_set_inline_array_size (const struct kan_reflection_field_t *array_field,
                                                  const void *owner_struct_instance,
                                                  kan_instance_size_t new_size)
{
    if (array_field->archetype_inline_array.size_field)
    {
        const struct kan_reflection_field_t *size_field = array_field->archetype_inline_array.size_field;
        const void *size_field_address = ((const uint8_t *) owner_struct_instance) + size_field->offset;

        switch (size_field->archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            switch (size_field->size)
            {
            case 1u:
                *(int8_t *) size_field_address = (int8_t) new_size;
            case 2u:
                *(int16_t *) size_field_address = (int16_t) new_size;
            case 4u:
                *(int32_t *) size_field_address = (int32_t) new_size;
            case 8u:
                *(int64_t *) size_field_address = (int64_t) new_size;
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            switch (size_field->size)
            {
            case 1u:
                *(uint8_t *) size_field_address = (uint8_t) new_size;
            case 2u:
                *(uint16_t *) size_field_address = (uint16_t) new_size;
            case 4u:
                *(uint32_t *) size_field_address = (uint32_t) new_size;
            case 8u:
                *(uint64_t *) size_field_address = (uint64_t) new_size;
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        case KAN_REFLECTION_ARCHETYPE_STRUCT:
        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_PATCH:
            KAN_ASSERT (KAN_FALSE)
            break;
        }
    }
}

kan_hash_t kan_reflection_hash_struct (kan_reflection_registry_t registry,
                                       const struct kan_reflection_struct_t *type,
                                       void *instance)
{
    KAN_ASSERT (type)
    kan_bool_t first_hash = KAN_TRUE;
    kan_hash_t hash = 0u;

#define APPEND_HASH(VALUE)                                                                                             \
    if (first_hash)                                                                                                    \
    {                                                                                                                  \
        hash = (kan_hash_t) (VALUE);                                                                                   \
        first_hash = KAN_FALSE;                                                                                        \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        hash = kan_hash_combine (hash, (kan_hash_t) (VALUE));                                                          \
    }

    struct kan_reflection_visibility_iterator_t iterator;
    kan_reflection_visibility_iterator_init (&iterator, type, instance);

    while (iterator.field != iterator.field_end)
    {
        void *address = ((uint8_t *) instance) + iterator.field->offset;
        switch (iterator.field->archetype)
        {
#define TRIVIAL_HASHES(ADDRESS, SIZE)                                                                                  \
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:                                                                          \
        switch (SIZE)                                                                                                  \
        {                                                                                                              \
        case 1u:                                                                                                       \
            APPEND_HASH (*(int8_t *) (ADDRESS));                                                                       \
            break;                                                                                                     \
        case 2u:                                                                                                       \
            APPEND_HASH (*(int16_t *) (ADDRESS));                                                                      \
            break;                                                                                                     \
        case 4u:                                                                                                       \
            APPEND_HASH (*(int32_t *) (ADDRESS));                                                                      \
            break;                                                                                                     \
        case 8u:                                                                                                       \
            APPEND_HASH (*(int64_t *) (ADDRESS));                                                                      \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:                                                                        \
    case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:                                                                    \
        switch (SIZE)                                                                                                  \
        {                                                                                                              \
        case 1u:                                                                                                       \
            APPEND_HASH (*(uint8_t *) (ADDRESS));                                                                      \
            break;                                                                                                     \
        case 2u:                                                                                                       \
            APPEND_HASH (*(uint16_t *) (ADDRESS));                                                                     \
            break;                                                                                                     \
        case 4u:                                                                                                       \
            APPEND_HASH (*(uint32_t *) (ADDRESS));                                                                     \
            break;                                                                                                     \
        case 8u:                                                                                                       \
            APPEND_HASH (*(uint64_t *) (ADDRESS));                                                                     \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_FLOATING:                                                                            \
        switch (SIZE)                                                                                                  \
        {                                                                                                              \
        case 4u:                                                                                                       \
            APPEND_HASH (*(float *) (ADDRESS));                                                                        \
            break;                                                                                                     \
        case 8u:                                                                                                       \
            APPEND_HASH (*(double *) (ADDRESS));                                                                       \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:                                                                      \
        APPEND_HASH (kan_string_hash (*(const char **) (ADDRESS)))                                                     \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:                                                                     \
        APPEND_HASH (*(kan_interned_string_t *) (ADDRESS));                                                            \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_ENUM:                                                                                \
        APPEND_HASH (*(int *) (ADDRESS))                                                                               \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:                                                                    \
    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:                                                                      \
        KAN_ASSERT (KAN_FALSE)                                                                                         \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_PATCH:                                                                               \
        APPEND_HASH (KAN_HANDLE_GET (*(kan_reflection_patch_t *) (ADDRESS)))                                           \
        break;

            TRIVIAL_HASHES (address, iterator.field->size)

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
            APPEND_HASH (kan_reflection_hash_struct (
                registry, kan_reflection_registry_query_struct (registry, iterator.field->archetype_struct.type_name),
                address));
            break;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        {
            const kan_instance_size_t size = kan_reflection_get_inline_array_size (iterator.field, address);
            for (kan_loop_size_t index = 0u; index < size; ++index)
            {
                void *item_address = ((uint8_t *) address) + iterator.field->archetype_inline_array.item_size * index;
                switch (iterator.field->archetype_dynamic_array.item_archetype)
                {
                    TRIVIAL_HASHES (item_address, iterator.field->archetype_inline_array.item_size)

                case KAN_REFLECTION_ARCHETYPE_STRUCT:
                    APPEND_HASH (kan_reflection_hash_struct (
                        registry,
                        kan_reflection_registry_query_struct (
                            registry, iterator.field->archetype_inline_array.item_archetype_struct.type_name),
                        item_address));
                    break;

                case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                    KAN_ASSERT (KAN_FALSE);
                    break;
                }
            }

            break;
        }

        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = address;
            for (kan_loop_size_t index = 0u; index < array->size; ++index)
            {
                void *item_address =
                    ((uint8_t *) array->data) + iterator.field->archetype_dynamic_array.item_size * index;
                switch (iterator.field->archetype_dynamic_array.item_archetype)
                {
                    TRIVIAL_HASHES (item_address, iterator.field->archetype_dynamic_array.item_size)

                case KAN_REFLECTION_ARCHETYPE_STRUCT:
                    APPEND_HASH (kan_reflection_hash_struct (
                        registry,
                        kan_reflection_registry_query_struct (
                            registry, iterator.field->archetype_dynamic_array.item_archetype_struct.type_name),
                        item_address));
                    break;

                case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                    KAN_ASSERT (KAN_FALSE);
                    break;
                }
            }

            break;
        }
        }

        kan_reflection_visibility_iterator_advance (&iterator);
    }

#undef TRIVIAL_HASHES
#undef APPEND_HASH
    return hash;
}

kan_bool_t kan_reflection_are_structs_equal (kan_reflection_registry_t registry,
                                             const struct kan_reflection_struct_t *type,
                                             void *first,
                                             void *second)
{
    struct kan_reflection_visibility_iterator_t first_iterator;
    kan_reflection_visibility_iterator_init (&first_iterator, type, first);

    struct kan_reflection_visibility_iterator_t second_iterator;
    kan_reflection_visibility_iterator_init (&second_iterator, type, second);

    while (first_iterator.field != first_iterator.field_end)
    {
        if (first_iterator.field != second_iterator.field)
        {
            return KAN_FALSE;
        }

        void *first_address = ((uint8_t *) first) + first_iterator.field->offset;
        void *second_address = ((uint8_t *) second) + second_iterator.field->offset;

#define CHECK_EQUALITY(FIRST, SECOND)                                                                                  \
    if ((FIRST) != (SECOND))                                                                                           \
    {                                                                                                                  \
        return KAN_FALSE;                                                                                              \
    }

        switch (first_iterator.field->archetype)
        {
#define TRIVIAL_EQUALITY(FIRST_ADDRESS, SECOND_ADDRESS, SIZE)                                                          \
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:                                                                          \
        switch (SIZE)                                                                                                  \
        {                                                                                                              \
        case 1u:                                                                                                       \
            CHECK_EQUALITY (*(int8_t *) (FIRST_ADDRESS), *(int8_t *) (SECOND_ADDRESS));                                \
            break;                                                                                                     \
        case 2u:                                                                                                       \
            CHECK_EQUALITY (*(int16_t *) (FIRST_ADDRESS), *(int16_t *) (SECOND_ADDRESS));                              \
            break;                                                                                                     \
        case 4u:                                                                                                       \
            CHECK_EQUALITY (*(int32_t *) (FIRST_ADDRESS), *(int32_t *) (SECOND_ADDRESS));                              \
            break;                                                                                                     \
        case 8u:                                                                                                       \
            CHECK_EQUALITY (*(int64_t *) (FIRST_ADDRESS), *(int64_t *) (SECOND_ADDRESS));                              \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:                                                                        \
    case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:                                                                    \
        switch (SIZE)                                                                                                  \
        {                                                                                                              \
        case 1u:                                                                                                       \
            CHECK_EQUALITY (*(uint8_t *) (FIRST_ADDRESS), *(uint8_t *) (SECOND_ADDRESS));                              \
            break;                                                                                                     \
        case 2u:                                                                                                       \
            CHECK_EQUALITY (*(uint16_t *) (FIRST_ADDRESS), *(uint16_t *) (SECOND_ADDRESS));                            \
            break;                                                                                                     \
        case 4u:                                                                                                       \
            CHECK_EQUALITY (*(uint32_t *) (FIRST_ADDRESS), *(uint32_t *) (SECOND_ADDRESS));                            \
            break;                                                                                                     \
        case 8u:                                                                                                       \
            CHECK_EQUALITY (*(uint64_t *) (FIRST_ADDRESS), *(uint64_t *) (SECOND_ADDRESS));                            \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_FLOATING:                                                                            \
        switch (SIZE)                                                                                                  \
        {                                                                                                              \
        case 4u:                                                                                                       \
            CHECK_EQUALITY (*(float *) (FIRST_ADDRESS), *(float *) (SECOND_ADDRESS));                                  \
            break;                                                                                                     \
        case 8u:                                                                                                       \
            CHECK_EQUALITY (*(double *) (FIRST_ADDRESS), *(double *) (SECOND_ADDRESS));                                \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:                                                                      \
        if ((FIRST_ADDRESS) != (SECOND_ADDRESS))                                                                       \
        {                                                                                                              \
            if (!(FIRST_ADDRESS) || !(SECOND_ADDRESS) ||                                                               \
                strcmp (*(const char **) (FIRST_ADDRESS), *(const char **) (SECOND_ADDRESS)) != 0)                     \
            {                                                                                                          \
                return KAN_FALSE;                                                                                      \
            }                                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:                                                                     \
        CHECK_EQUALITY (*(kan_interned_string_t *) (FIRST_ADDRESS), *(kan_interned_string_t *) (SECOND_ADDRESS));      \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_ENUM:                                                                                \
        CHECK_EQUALITY (*(int *) (FIRST_ADDRESS), *(int *) (SECOND_ADDRESS));                                          \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:                                                                    \
    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:                                                                      \
        KAN_ASSERT (KAN_FALSE)                                                                                         \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_PATCH:                                                                               \
        if (!KAN_HANDLE_IS_EQUAL (*(kan_reflection_patch_t *) (FIRST_ADDRESS),                                         \
                                  *(kan_reflection_patch_t *) (SECOND_ADDRESS)))                                       \
        {                                                                                                              \
            return KAN_FALSE;                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        break;

            TRIVIAL_EQUALITY (first_address, second_address, first_iterator.field->size)

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
            if (!kan_reflection_are_structs_equal (
                    registry,
                    kan_reflection_registry_query_struct (registry, first_iterator.field->archetype_struct.type_name),
                    first_address, second_address))
            {
                return KAN_FALSE;
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        {
            const kan_instance_size_t first_size =
                kan_reflection_get_inline_array_size (first_iterator.field, first_address);
            const kan_instance_size_t second_size =
                kan_reflection_get_inline_array_size (first_iterator.field, second_address);

            if (first_size != second_size)
            {
                return KAN_FALSE;
            }

            for (kan_loop_size_t index = 0u; index < first_size; ++index)
            {
                void *first_item_address =
                    ((uint8_t *) first_address) + first_iterator.field->archetype_inline_array.item_size * index;
                void *second_item_address =
                    ((uint8_t *) second_address) + second_iterator.field->archetype_inline_array.item_size * index;

                switch (first_iterator.field->archetype_dynamic_array.item_archetype)
                {
                    TRIVIAL_EQUALITY (first_item_address, second_item_address,
                                      first_iterator.field->archetype_inline_array.item_size)

                case KAN_REFLECTION_ARCHETYPE_STRUCT:
                    if (!kan_reflection_are_structs_equal (
                            registry,
                            kan_reflection_registry_query_struct (
                                registry, first_iterator.field->archetype_inline_array.item_archetype_struct.type_name),
                            first_item_address, second_item_address))
                    {
                        return KAN_FALSE;
                    }

                    break;

                case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                    KAN_ASSERT (KAN_FALSE);
                    break;
                }
            }

            break;
        }

        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *first_array = first_address;
            struct kan_dynamic_array_t *second_array = second_address;

            if (first_array->size != second_array->size)
            {
                return KAN_FALSE;
            }

            for (kan_loop_size_t index = 0u; index < first_array->size; ++index)
            {
                void *first_item_address =
                    ((uint8_t *) first_array->data) + first_iterator.field->archetype_dynamic_array.item_size * index;
                void *second_item_address =
                    ((uint8_t *) second_array->data) + second_iterator.field->archetype_dynamic_array.item_size * index;

                switch (first_iterator.field->archetype_dynamic_array.item_archetype)
                {
                    TRIVIAL_EQUALITY (first_item_address, second_item_address,
                                      first_iterator.field->archetype_dynamic_array.item_size)

                case KAN_REFLECTION_ARCHETYPE_STRUCT:
                    if (!kan_reflection_are_structs_equal (
                            registry,
                            kan_reflection_registry_query_struct (
                                registry,
                                first_iterator.field->archetype_dynamic_array.item_archetype_struct.type_name),
                            first_item_address, second_item_address))
                    {
                        return KAN_FALSE;
                    }

                    break;

                case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                    KAN_ASSERT (KAN_FALSE);
                    break;
                }
            }

            break;
        }
        }

        kan_reflection_visibility_iterator_advance (&first_iterator);
        kan_reflection_visibility_iterator_advance (&second_iterator);
    }

#undef TRIVIAL_EQUALITY
#undef CHECK_EQUALITY

    return first_iterator.field == second_iterator.field;
}

void kan_reflection_move_struct (kan_reflection_registry_t registry,
                                 const struct kan_reflection_struct_t *type,
                                 void *target,
                                 void *source)
{
    struct kan_reflection_visibility_iterator_t iterator;
    kan_reflection_visibility_iterator_init (&iterator, type, source);

    while (iterator.field != iterator.field_end)
    {
        void *target_address = ((uint8_t *) target) + iterator.field->offset;
        void *source_address = ((uint8_t *) source) + iterator.field->offset;

        switch (iterator.field->archetype)
        {
#define TRIVIAL_MOVE(TARGET_ADDRESS, SOURCE_ADDRESS, SIZE)                                                             \
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:                                                                          \
        switch (SIZE)                                                                                                  \
        {                                                                                                              \
        case 1u:                                                                                                       \
            *(int8_t *) (TARGET_ADDRESS) = *(int8_t *) (SOURCE_ADDRESS);                                               \
            break;                                                                                                     \
        case 2u:                                                                                                       \
            *(int16_t *) (TARGET_ADDRESS) = *(int16_t *) (SOURCE_ADDRESS);                                             \
            break;                                                                                                     \
        case 4u:                                                                                                       \
            *(int32_t *) (TARGET_ADDRESS) = *(int32_t *) (SOURCE_ADDRESS);                                             \
            break;                                                                                                     \
        case 8u:                                                                                                       \
            *(int64_t *) (TARGET_ADDRESS) = *(int64_t *) (SOURCE_ADDRESS);                                             \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:                                                                        \
        switch (SIZE)                                                                                                  \
        {                                                                                                              \
        case 1u:                                                                                                       \
            *(uint8_t *) (TARGET_ADDRESS) = *(uint8_t *) (SOURCE_ADDRESS);                                             \
            break;                                                                                                     \
        case 2u:                                                                                                       \
            *(uint16_t *) (TARGET_ADDRESS) = *(uint16_t *) (SOURCE_ADDRESS);                                           \
            break;                                                                                                     \
        case 4u:                                                                                                       \
            *(uint32_t *) (TARGET_ADDRESS) = *(uint32_t *) (SOURCE_ADDRESS);                                           \
            break;                                                                                                     \
        case 8u:                                                                                                       \
            *(uint64_t *) (TARGET_ADDRESS) = *(uint64_t *) (SOURCE_ADDRESS);                                           \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_FLOATING:                                                                            \
        switch (SIZE)                                                                                                  \
        {                                                                                                              \
        case 4u:                                                                                                       \
            *(float *) (TARGET_ADDRESS) = *(float *) (SOURCE_ADDRESS);                                                 \
            break;                                                                                                     \
        case 8u:                                                                                                       \
            *(double *) (TARGET_ADDRESS) = *(double *) (SOURCE_ADDRESS);                                               \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:                                                                    \
        switch (SIZE)                                                                                                  \
        {                                                                                                              \
        case 1u:                                                                                                       \
            *(uint8_t *) (TARGET_ADDRESS) = *(uint8_t *) (SOURCE_ADDRESS);                                             \
            *(uint8_t *) (SOURCE_ADDRESS) = 0u;                                                                        \
            break;                                                                                                     \
        case 2u:                                                                                                       \
            *(uint16_t *) (TARGET_ADDRESS) = *(uint16_t *) (SOURCE_ADDRESS);                                           \
            *(uint16_t *) (SOURCE_ADDRESS) = 0u;                                                                       \
            break;                                                                                                     \
        case 4u:                                                                                                       \
            *(uint32_t *) (TARGET_ADDRESS) = *(uint32_t *) (SOURCE_ADDRESS);                                           \
            *(uint32_t *) (SOURCE_ADDRESS) = 0u;                                                                       \
            break;                                                                                                     \
        case 8u:                                                                                                       \
            *(uint64_t *) (TARGET_ADDRESS) = *(uint64_t *) (SOURCE_ADDRESS);                                           \
            *(uint64_t *) (SOURCE_ADDRESS) = 0u;                                                                       \
            break;                                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:                                                                      \
        *(char **) (TARGET_ADDRESS) = *(char **) (SOURCE_ADDRESS);                                                     \
        *(char **) (SOURCE_ADDRESS) = NULL;                                                                            \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:                                                                     \
        *(kan_interned_string_t *) (TARGET_ADDRESS) = *(kan_interned_string_t *) (SOURCE_ADDRESS);                     \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_ENUM:                                                                                \
        *(int *) (TARGET_ADDRESS) = *(int *) (SOURCE_ADDRESS);                                                         \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:                                                                    \
    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:                                                                      \
        KAN_ASSERT (KAN_FALSE)                                                                                         \
        break;                                                                                                         \
                                                                                                                       \
    case KAN_REFLECTION_ARCHETYPE_PATCH:                                                                               \
        *(kan_reflection_patch_t *) (TARGET_ADDRESS) = *(kan_reflection_patch_t *) (SOURCE_ADDRESS);                   \
        *(kan_reflection_patch_t *) (SOURCE_ADDRESS) = KAN_HANDLE_SET_INVALID (kan_reflection_patch_t);                \
        break;

            TRIVIAL_MOVE (target_address, source_address, iterator.field->size)

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
            kan_reflection_move_struct (
                registry, kan_reflection_registry_query_struct (registry, iterator.field->archetype_struct.type_name),
                target_address, source_address);
            break;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        {
            const kan_instance_size_t size = kan_reflection_get_inline_array_size (iterator.field, source_address);
            for (kan_loop_size_t index = 0u; index < size; ++index)
            {
                void *target_item_address =
                    ((uint8_t *) target_address) + iterator.field->archetype_inline_array.item_size * index;
                void *source_item_address =
                    ((uint8_t *) source_address) + iterator.field->archetype_inline_array.item_size * index;

                switch (iterator.field->archetype_dynamic_array.item_archetype)
                {
                    TRIVIAL_MOVE (target_item_address, source_item_address,
                                  iterator.field->archetype_inline_array.item_size)

                case KAN_REFLECTION_ARCHETYPE_STRUCT:
                    kan_reflection_move_struct (
                        registry,
                        kan_reflection_registry_query_struct (
                            registry, iterator.field->archetype_inline_array.item_archetype_struct.type_name),
                        target_item_address, source_item_address);
                    break;

                case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                    KAN_ASSERT (KAN_FALSE);
                    break;
                }
            }

            break;
        }

        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *target_array = target_address;
            struct kan_dynamic_array_t *source_array = source_address;
            *target_array = *source_array;
            source_array->size = 0u;
            source_array->capacity = 0u;
            source_array->data = NULL;
        }
        }

        kan_reflection_visibility_iterator_advance (&iterator);
    }

#undef TRIVIAL_MOVE
#undef CHECK_MOVE
}

void kan_reflection_reset_struct (kan_reflection_registry_t registry,
                                  const struct kan_reflection_struct_t *type,
                                  void *instance)
{
    struct kan_reflection_visibility_iterator_t iterator;
    kan_reflection_visibility_iterator_init (&iterator, type, instance);

    while (iterator.field != iterator.field_end)
    {
        void *address = ((uint8_t *) instance) + iterator.field->offset;
        switch (iterator.field->archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
            break;

        case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
            switch (iterator.field->size)
            {
            case 1u:
                *(uint8_t *) address = 0u;
                break;
            case 2u:
                *(uint16_t *) address = 0u;
                break;
            case 4u:
                *(uint32_t *) address = 0u;
                break;
            case 8u:
                *(uint64_t *) address = 0u;
                break;
            }

            break;

            // There pointers are unsupported as we don't know how exactly we should reset them.
        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            // Patches are not supported either as we don't know whether to destroy or invalidate them.
        case KAN_REFLECTION_ARCHETYPE_PATCH:
            KAN_ASSERT (KAN_FALSE)
            break;

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
            kan_reflection_reset_struct (
                registry, kan_reflection_registry_query_struct (registry, iterator.field->archetype_struct.type_name),
                address);
            break;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        {
            kan_instance_size_t size = kan_reflection_get_inline_array_size (iterator.field, instance);
            kan_reflection_set_inline_array_size (iterator.field, instance, 0u);

            if (iterator.field->archetype_inline_array.item_archetype == KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL)
            {
                memset (address, 0u, size * iterator.field->archetype_inline_array.item_size);
            }
            else if (iterator.field->archetype_inline_array.item_archetype == KAN_REFLECTION_ARCHETYPE_STRUCT)
            {
                const struct kan_reflection_struct_t *item_type = kan_reflection_registry_query_struct (
                    registry, iterator.field->archetype_inline_array.item_archetype_struct.type_name);

                if (item_type->shutdown)
                {
                    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) size; ++index)
                    {
                        item_type->shutdown (
                            item_type->functor_user_data,
                            ((uint8_t *) address) + iterator.field->archetype_inline_array.item_size * index);
                    }
                }
            }

            break;
        }

        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = address;
            if (iterator.field->archetype_dynamic_array.item_archetype == KAN_REFLECTION_ARCHETYPE_STRUCT)
            {
                const struct kan_reflection_struct_t *item_type = kan_reflection_registry_query_struct (
                    registry, iterator.field->archetype_dynamic_array.item_archetype_struct.type_name);

                if (item_type->shutdown)
                {
                    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) array->size; ++index)
                    {
                        item_type->shutdown (item_type->functor_user_data,
                                             ((uint8_t *) array->data) + array->item_size * index);
                    }
                }
            }

            array->size = 0u;
            break;
        }
        }

        kan_reflection_visibility_iterator_advance (&iterator);
    }
}
