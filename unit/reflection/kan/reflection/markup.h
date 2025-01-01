#pragma once

#include <kan/api_common/highlight.h>
#include <kan/api_common/make_pragma.h>

/// \file
/// \brief Contains macros for reflection data markup in sources.

/// \brief Marks next enum as enumeration of flags.
#define KAN_REFLECTION_FLAGS KAN_MAKE_PRAGMA (kan_reflection_flags)

/// \brief Marks next bit of info (enum, enum value, struct, struct field, function, function argument)
///        as ignored by reflection.
#define KAN_REFLECTION_IGNORE KAN_MAKE_PRAGMA (kan_reflection_ignore)

/// \brief Marks next field as external pointer archetype.
#define KAN_REFLECTION_EXTERNAL_POINTER KAN_MAKE_PRAGMA (kan_reflection_external_pointer)

/// \brief Specifies explicit init functor for structure.
/// \details Usually only needed for specific testing. Implicit functors are advised for general use cases instead.
#define KAN_REFLECTION_EXPLICIT_INIT_FUNCTOR(FUNCTION_NAME)                                                            \
    KAN_HIGHLIGHT_SIZEOF_POSSIBLE (FUNCTION_NAME) KAN_MAKE_PRAGMA (kan_reflection_explicit_init_functor FUNCTION_NAME)

/// \brief Specifies explicit shutdown functor for structure.
/// \details Usually only needed for specific testing. Implicit functors are advised for general use cases instead.
#define KAN_REFLECTION_EXPLICIT_SHUTDOWN_FUNCTOR(FUNCTION_NAME)                                                        \
    KAN_HIGHLIGHT_SIZEOF_POSSIBLE (FUNCTION_NAME)                                                                      \
    KAN_MAKE_PRAGMA (kan_reflection_explicit_shutdown_functor FUNCTION_NAME)

/// \brief Informs that next dynamic array contains items of given type. Required for dynamic arrays.
#define KAN_REFLECTION_DYNAMIC_ARRAY_TYPE(TYPE)                                                                        \
    KAN_HIGHLIGHT_SIZEOF_POSSIBLE (TYPE) KAN_MAKE_PRAGMA (kan_reflection_dynamic_array_type TYPE)

/// \brief Informs that actual size of the next inplace array is stored in field with given name.
#define KAN_REFLECTION_SIZE_FIELD(FIELD) KAN_MAKE_PRAGMA (kan_reflection_size_field FIELD)

/// \brief Attaches next field visibility to the value of field with given name.
#define KAN_REFLECTION_VISIBILITY_CONDITION_FIELD(FIELD)                                                               \
    KAN_MAKE_PRAGMA (kan_reflection_visibility_condition_field FIELD)

/// \brief Adds value that enables visibility of the next field for visibility condition feature.
#define KAN_REFLECTION_VISIBILITY_CONDITION_VALUE(VALUE)                                                               \
    KAN_HIGHLIGHT_SIZEOF_POSSIBLE (FUNCTION_NAME) KAN_MAKE_PRAGMA (kan_reflection_visibility_condition_value VALUE)

/// \brief Marks next symbol as meta for enum with given name.
#define KAN_REFLECTION_ENUM_META(ENUM_NAME)                                                                            \
    KAN_HIGHLIGHT_ENUM_NAME (ENUM_NAME) KAN_MAKE_PRAGMA (kan_reflection_enum_meta ENUM_NAME)

/// \brief Marks next symbol as meta for given enum value of given enum.
#define KAN_REFLECTION_ENUM_VALUE_META(ENUM_NAME, ENUM_VALUE_NAME)                                                     \
    KAN_HIGHLIGHT_SIZEOF_POSSIBLE (FUNCTION_NAME)                                                                      \
    KAN_MAKE_PRAGMA (kan_reflection_enum_value_meta ENUM_NAME ENUM_VALUE_NAME)

/// \brief Marks next symbol as meta for struct with given name.
#define KAN_REFLECTION_STRUCT_META(STRUCT_NAME)                                                                        \
    KAN_HIGHLIGHT_STRUCT_NAME (STRUCT_NAME) KAN_MAKE_PRAGMA (kan_reflection_struct_meta STRUCT_NAME)

/// \brief Marks next symbol as meta for given struct field of given struct.
#define KAN_REFLECTION_STRUCT_FIELD_META(STRUCT_NAME, STRUCT_FIELD_NAME)                                               \
    KAN_HIGHLIGHT_STRUCT_FIELD (STRUCT_NAME, STRUCT_FIELD_NAME)                                                        \
    KAN_MAKE_PRAGMA (kan_reflection_struct_field_meta STRUCT_NAME STRUCT_FIELD_NAME)

/// \brief Marks next symbol as meta for function with given name.
#define KAN_REFLECTION_FUNCTION_META(FUNCTION_NAME) KAN_MAKE_PRAGMA (kan_reflection_function_meta FUNCTION_NAME)

/// \brief Marks next symbol as meta for given function argument of given function.
#define KAN_REFLECTION_FUNCTION_ARGUMENT_META(FUNCTION_NAME, FUNCTION_ARGUMENT_NAME)                                   \
    KAN_MAKE_PRAGMA (kan_reflection_function_argument_meta FUNCTION_NAME FUNCTION_ARGUMENT_NAME)

/// \brief Allows to explicitly override registered enum/struct/function name.
/// \details It is only aimed for specific cases in tests and should not be used in real projects.
///          As it is only changes registration name and nothing else, most implicit features do not apply.
///          But meta should use new explicit name instead of the real one.
#define KAN_REFLECTION_EXPLICIT_REGISTRATION_NAME(NAME) KAN_MAKE_PRAGMA (kan_reflection_explicit_registration_name NAME)
