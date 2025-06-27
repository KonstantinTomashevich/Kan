#pragma once

/// \file
/// \brief Contains macros that are used to give additional information about macro parameters to the IDE
///        in order to make better autocomplete possible.

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
/// \brief Macro that tells IDE that autocomplete should show everything for which sizeof is possible.
#    define KAN_HIGHLIGHT_SIZEOF_POSSIBLE(ARGUMENT) static_assert (sizeof (ARGUMENT) != 0u, "Highlight possible.");

/// \brief Macro that tells IDE that autocomplete should show enum names (argument should not have enum prefix).
#    define KAN_HIGHLIGHT_ENUM_NAME(ARGUMENT) static_assert (sizeof (enum ARGUMENT) != 0u, "Highlight possible.");

/// \brief Macro that tells IDE that autocomplete should show struct names (argument should not have struct prefix).
#    define KAN_HIGHLIGHT_STRUCT_NAME(ARGUMENT) static_assert (sizeof (struct ARGUMENT) != 0u, "Highlight possible.");

/// \brief Macro that tells IDE that autocomplete should show struct field name.
#    define KAN_HIGHLIGHT_STRUCT_FIELD(STRUCT, FIELD)                                                                  \
        static_assert ((sizeof ((struct STRUCT *) NULL)->FIELD) != 0u, "Highlight possible.");
#else
#    define KAN_HIGHLIGHT_SIZEOF_POSSIBLE(...)
#    define KAN_HIGHLIGHT_ENUM_NAME(...)
#    define KAN_HIGHLIGHT_STRUCT_NAME(...)
#    define KAN_HIGHLIGHT_STRUCT_FIELD(...)
#endif
