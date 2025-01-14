#pragma once

#include <platform_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/reflection/markup.h>

/// \file
/// \brief Provides basic API for platform-specific keyboard data.

KAN_C_HEADER_BEGIN

/// \brief Enumerates keyboard button scancodes that can be known to platform.
/// \details Scan code describes physical key on keyboard independently of layout selected by system.
enum kan_platform_scan_code_t
{
    KAN_PLATFORM_SCAN_CODE_UNKNOWN = 0u,

    KAN_PLATFORM_SCAN_CODE_A,
    KAN_PLATFORM_SCAN_CODE_B,
    KAN_PLATFORM_SCAN_CODE_C,
    KAN_PLATFORM_SCAN_CODE_D,
    KAN_PLATFORM_SCAN_CODE_E,
    KAN_PLATFORM_SCAN_CODE_F,
    KAN_PLATFORM_SCAN_CODE_G,
    KAN_PLATFORM_SCAN_CODE_H,
    KAN_PLATFORM_SCAN_CODE_I,
    KAN_PLATFORM_SCAN_CODE_J,
    KAN_PLATFORM_SCAN_CODE_K,
    KAN_PLATFORM_SCAN_CODE_L,
    KAN_PLATFORM_SCAN_CODE_M,
    KAN_PLATFORM_SCAN_CODE_N,
    KAN_PLATFORM_SCAN_CODE_O,
    KAN_PLATFORM_SCAN_CODE_P,
    KAN_PLATFORM_SCAN_CODE_Q,
    KAN_PLATFORM_SCAN_CODE_R,
    KAN_PLATFORM_SCAN_CODE_S,
    KAN_PLATFORM_SCAN_CODE_T,
    KAN_PLATFORM_SCAN_CODE_U,
    KAN_PLATFORM_SCAN_CODE_V,
    KAN_PLATFORM_SCAN_CODE_W,
    KAN_PLATFORM_SCAN_CODE_X,
    KAN_PLATFORM_SCAN_CODE_Y,
    KAN_PLATFORM_SCAN_CODE_Z,

    KAN_PLATFORM_SCAN_CODE_1,
    KAN_PLATFORM_SCAN_CODE_2,
    KAN_PLATFORM_SCAN_CODE_3,
    KAN_PLATFORM_SCAN_CODE_4,
    KAN_PLATFORM_SCAN_CODE_5,
    KAN_PLATFORM_SCAN_CODE_6,
    KAN_PLATFORM_SCAN_CODE_7,
    KAN_PLATFORM_SCAN_CODE_8,
    KAN_PLATFORM_SCAN_CODE_9,
    KAN_PLATFORM_SCAN_CODE_0,

    KAN_PLATFORM_SCAN_CODE_ENTER,
    KAN_PLATFORM_SCAN_CODE_ESCAPE,
    KAN_PLATFORM_SCAN_CODE_BACKSPACE,
    KAN_PLATFORM_SCAN_CODE_TAB,
    KAN_PLATFORM_SCAN_CODE_SPACE,

    KAN_PLATFORM_SCAN_CODE_MINUS,
    KAN_PLATFORM_SCAN_CODE_EQUAL,
    KAN_PLATFORM_SCAN_CODE_LEFT_BRACKET,
    KAN_PLATFORM_SCAN_CODE_RIGHT_BRACKET,
    KAN_PLATFORM_SCAN_CODE_BACKSLASH,
    KAN_PLATFORM_SCAN_CODE_NONUS_HASH,
    KAN_PLATFORM_SCAN_CODE_SEMICOLON,
    KAN_PLATFORM_SCAN_CODE_APOSTROPHE,
    KAN_PLATFORM_SCAN_CODE_GRAVE,
    KAN_PLATFORM_SCAN_CODE_COMMA,
    KAN_PLATFORM_SCAN_CODE_PERIOD,
    KAN_PLATFORM_SCAN_CODE_SLASH,

    KAN_PLATFORM_SCAN_CODE_CAPSLOCK,

    KAN_PLATFORM_SCAN_CODE_F1,
    KAN_PLATFORM_SCAN_CODE_F2,
    KAN_PLATFORM_SCAN_CODE_F3,
    KAN_PLATFORM_SCAN_CODE_F4,
    KAN_PLATFORM_SCAN_CODE_F5,
    KAN_PLATFORM_SCAN_CODE_F6,
    KAN_PLATFORM_SCAN_CODE_F7,
    KAN_PLATFORM_SCAN_CODE_F8,
    KAN_PLATFORM_SCAN_CODE_F9,
    KAN_PLATFORM_SCAN_CODE_F10,
    KAN_PLATFORM_SCAN_CODE_F11,
    KAN_PLATFORM_SCAN_CODE_F12,

    KAN_PLATFORM_SCAN_CODE_PRINT_SCREEN,
    KAN_PLATFORM_SCAN_CODE_SCROLL_LOCK,
    KAN_PLATFORM_SCAN_CODE_PAUSE,
    KAN_PLATFORM_SCAN_CODE_INSERT,
    KAN_PLATFORM_SCAN_CODE_HOME,
    KAN_PLATFORM_SCAN_CODE_PAGE_UP,
    KAN_PLATFORM_SCAN_CODE_DELETE,
    KAN_PLATFORM_SCAN_CODE_END,
    KAN_PLATFORM_SCAN_CODE_PAGE_DOWN,
    KAN_PLATFORM_SCAN_CODE_RIGHT,
    KAN_PLATFORM_SCAN_CODE_LEFT,
    KAN_PLATFORM_SCAN_CODE_DOWN,
    KAN_PLATFORM_SCAN_CODE_UP,

    KAN_PLATFORM_SCAN_CODE_NUM_LOCK,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_DIVIDE,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_MULTIPLY,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_MINUS,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_PLUS,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_ENTER,

    KAN_PLATFORM_SCAN_CODE_KEY_PAD_1,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_2,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_3,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_4,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_5,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_6,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_7,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_8,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_9,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_0,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_PERIOD,

    KAN_PLATFORM_SCAN_CODE_NONUS_BACKSLASH,
    KAN_PLATFORM_SCAN_CODE_APPLICATION,
    KAN_PLATFORM_SCAN_CODE_POWER,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_EQUALS,
    KAN_PLATFORM_SCAN_CODE_F13,
    KAN_PLATFORM_SCAN_CODE_F14,
    KAN_PLATFORM_SCAN_CODE_F15,
    KAN_PLATFORM_SCAN_CODE_F16,
    KAN_PLATFORM_SCAN_CODE_F17,
    KAN_PLATFORM_SCAN_CODE_F18,
    KAN_PLATFORM_SCAN_CODE_F19,
    KAN_PLATFORM_SCAN_CODE_F20,
    KAN_PLATFORM_SCAN_CODE_F21,
    KAN_PLATFORM_SCAN_CODE_F22,
    KAN_PLATFORM_SCAN_CODE_F23,
    KAN_PLATFORM_SCAN_CODE_F24,
    KAN_PLATFORM_SCAN_CODE_EXECUTE,
    KAN_PLATFORM_SCAN_CODE_HELP,
    KAN_PLATFORM_SCAN_CODE_MENU,
    KAN_PLATFORM_SCAN_CODE_SELECT,
    KAN_PLATFORM_SCAN_CODE_STOP,
    KAN_PLATFORM_SCAN_CODE_AGAIN,
    KAN_PLATFORM_SCAN_CODE_UNDO,
    KAN_PLATFORM_SCAN_CODE_CUT,
    KAN_PLATFORM_SCAN_CODE_COPY,
    KAN_PLATFORM_SCAN_CODE_PASTE,
    KAN_PLATFORM_SCAN_CODE_FIND,
    KAN_PLATFORM_SCAN_CODE_MUTE,
    KAN_PLATFORM_SCAN_CODE_VOLUME_UP,
    KAN_PLATFORM_SCAN_CODE_VOLUME_DOWN,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_COMMA,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_EQUALS_AS400,

    KAN_PLATFORM_SCAN_CODE_INTERNATIONAL_1,
    KAN_PLATFORM_SCAN_CODE_INTERNATIONAL_2,
    KAN_PLATFORM_SCAN_CODE_INTERNATIONAL_3,
    KAN_PLATFORM_SCAN_CODE_INTERNATIONAL_4,
    KAN_PLATFORM_SCAN_CODE_INTERNATIONAL_5,
    KAN_PLATFORM_SCAN_CODE_INTERNATIONAL_6,
    KAN_PLATFORM_SCAN_CODE_INTERNATIONAL_7,
    KAN_PLATFORM_SCAN_CODE_INTERNATIONAL_8,
    KAN_PLATFORM_SCAN_CODE_INTERNATIONAL_9,

    KAN_PLATFORM_SCAN_CODE_LANG_1,
    KAN_PLATFORM_SCAN_CODE_LANG_2,
    KAN_PLATFORM_SCAN_CODE_LANG_3,
    KAN_PLATFORM_SCAN_CODE_LANG_4,
    KAN_PLATFORM_SCAN_CODE_LANG_5,
    KAN_PLATFORM_SCAN_CODE_LANG_6,
    KAN_PLATFORM_SCAN_CODE_LANG_7,
    KAN_PLATFORM_SCAN_CODE_LANG_8,
    KAN_PLATFORM_SCAN_CODE_LANG_9,

    KAN_PLATFORM_SCAN_CODE_ALT_ERASE,
    KAN_PLATFORM_SCAN_CODE_SYS_REQ,
    KAN_PLATFORM_SCAN_CODE_CANCEL,
    KAN_PLATFORM_SCAN_CODE_CLEAR,
    KAN_PLATFORM_SCAN_CODE_PRIOR,
    KAN_PLATFORM_SCAN_CODE_RETURN_2,
    KAN_PLATFORM_SCAN_CODE_SEPARATOR,
    KAN_PLATFORM_SCAN_CODE_OUT,
    KAN_PLATFORM_SCAN_CODE_OPER,
    KAN_PLATFORM_SCAN_CODE_CLEAR_AGAIN,

    KAN_PLATFORM_SCAN_CODE_CR_SEL,
    KAN_PLATFORM_SCAN_CODE_EX_SEL,

    KAN_PLATFORM_SCAN_CODE_KEY_PAD_00,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_000,
    KAN_PLATFORM_SCAN_CODE_THOUSANDS_SEPARATOR,
    KAN_PLATFORM_SCAN_CODE_DECIMAL_SEPARATOR,
    KAN_PLATFORM_SCAN_CODE_CURRENCY_UNIT,
    KAN_PLATFORM_SCAN_CODE_CURRENCY_SUBUNIT,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_LEFT_PAREN,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_RIGHT_PAREN,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_LEFT_BRACE,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_RIGHT_BRACE,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_TAB,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_BACKSPACE,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_A,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_B,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_C,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_D,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_E,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_F,

    KAN_PLATFORM_SCAN_CODE_KEY_PAD_XOR,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_POWER,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_PERCENT,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_LESS,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_GREATER,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_AMPERSAND,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_DOUBLE_AMPERSAND,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_VERTICAL_BAR,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_DOUBLE_VERTICAL_BAR,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_COLON,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_HASH,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_SPACE,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_AT,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_EXCLAMATION,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_MEM_STORE,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_MEM_RECALL,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_MEM_CLEAR,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_MEM_ADD,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_MEM_SUBTRACT,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_MEM_MULTIPLY,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_MEM_DIVIDE,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_PLUS_MINUS,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_CLEAR,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_CLEAR_ENTRY,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_BINARY,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_OCTAL,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_DECIMAL,
    KAN_PLATFORM_SCAN_CODE_KEY_PAD_HEXADECIMAL,

    KAN_PLATFORM_SCAN_CODE_LEFT_CONTROL,
    KAN_PLATFORM_SCAN_CODE_LEFT_SHIFT,
    KAN_PLATFORM_SCAN_CODE_LEFT_ALT,
    KAN_PLATFORM_SCAN_CODE_LEFT_GUI,
    KAN_PLATFORM_SCAN_CODE_RIGHT_CONTROL,
    KAN_PLATFORM_SCAN_CODE_RIGHT_SHIFT,
    KAN_PLATFORM_SCAN_CODE_RIGHT_ALT,
    KAN_PLATFORM_SCAN_CODE_RIGHT_GUI,

    KAN_PLATFORM_SCAN_CODE_MODE,

    KAN_PLATFORM_SCAN_CODE_MEDIA_NEXT,
    KAN_PLATFORM_SCAN_CODE_MEDIA_PREVIOUS,
    KAN_PLATFORM_SCAN_CODE_MEDIA_STOP,
    KAN_PLATFORM_SCAN_CODE_MEDIA_PLAY,
    KAN_PLATFORM_SCAN_CODE_MEDIA_SELECT,

    KAN_PLATFORM_SCAN_CODE_AC_SEARCH,
    KAN_PLATFORM_SCAN_CODE_AC_HOME,
    KAN_PLATFORM_SCAN_CODE_AC_BACK,
    KAN_PLATFORM_SCAN_CODE_AC_FORWARD,
    KAN_PLATFORM_SCAN_CODE_AC_STOP,
    KAN_PLATFORM_SCAN_CODE_AC_REFRESH,
    KAN_PLATFORM_SCAN_CODE_AC_BOOKMARKS,

    KAN_PLATFORM_SCAN_CODE_EJECT,
    KAN_PLATFORM_SCAN_CODE_SLEEP,

    KAN_PLATFORM_SCAN_CODE_MEDIA_REWIND,
    KAN_PLATFORM_SCAN_CODE_MEDIA_FAST_FORWARD,

    KAN_PLATFORM_SCAN_CODE_SOFT_LEFT,
    KAN_PLATFORM_SCAN_CODE_SOFT_RIGHT,

    KAN_PLATFORM_SCAN_CODE_CALL,
    KAN_PLATFORM_SCAN_CODE_END_CALL,
};

/// \brief Type for storing key code value in Unicode format.
/// \details Key codes depend on layout selected for the keyboard by the system.
typedef uint32_t kan_platform_key_code_t;

/// \brief Table that lists modifier keys as flags that can be known to platform.
KAN_REFLECTION_FLAGS
enum kan_platform_modifier_mask_t
{
    KAN_PLATFORM_MODIFIER_MASK_LEFT_SHIFT = 1u << 0u,
    KAN_PLATFORM_MODIFIER_MASK_RIGHT_SHIFT = 1u << 1u,
    KAN_PLATFORM_MODIFIER_MASK_LEFT_CONTROL = 1u << 2u,
    KAN_PLATFORM_MODIFIER_MASK_RIGHT_CONTROL = 1u << 3u,
    KAN_PLATFORM_MODIFIER_MASK_LEFT_ALT = 1u << 4u,
    KAN_PLATFORM_MODIFIER_MASK_RIGHT_ALT = 1u << 5u,
    KAN_PLATFORM_MODIFIER_MASK_LEFT_GUI = 1u << 6u,
    KAN_PLATFORM_MODIFIER_MASK_RIGHT_GUI = 1u << 7u,
    KAN_PLATFORM_MODIFIER_MASK_NUM_LOCK = 1u << 8u,
    KAN_PLATFORM_MODIFIER_MASK_CAPS_LOCK = 1u << 9u,
    KAN_PLATFORM_MODIFIER_MASK_MODE = 1u << 10u,
    KAN_PLATFORM_MODIFIER_MASK_SCROLL_LOCK = 1u << 11u,
};

KAN_C_HEADER_END
