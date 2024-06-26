#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>

#include <kan/platform/keyboard.h>

static struct kan_scan_code_table_t scan_code_table = {
    .a = SDL_SCANCODE_A,
    .b = SDL_SCANCODE_B,
    .c = SDL_SCANCODE_C,
    .d = SDL_SCANCODE_D,
    .e = SDL_SCANCODE_E,
    .f = SDL_SCANCODE_F,
    .g = SDL_SCANCODE_G,
    .h = SDL_SCANCODE_H,
    .i = SDL_SCANCODE_I,
    .j = SDL_SCANCODE_J,
    .k = SDL_SCANCODE_K,
    .l = SDL_SCANCODE_L,
    .m = SDL_SCANCODE_M,
    .n = SDL_SCANCODE_N,
    .o = SDL_SCANCODE_O,
    .p = SDL_SCANCODE_P,
    .q = SDL_SCANCODE_Q,
    .r = SDL_SCANCODE_R,
    .s = SDL_SCANCODE_S,
    .t = SDL_SCANCODE_T,
    .u = SDL_SCANCODE_U,
    .v = SDL_SCANCODE_V,
    .w = SDL_SCANCODE_W,
    .x = SDL_SCANCODE_X,
    .y = SDL_SCANCODE_Y,
    .z = SDL_SCANCODE_Z,
    ._1 = SDL_SCANCODE_1,
    ._2 = SDL_SCANCODE_2,
    ._3 = SDL_SCANCODE_3,
    ._4 = SDL_SCANCODE_4,
    ._5 = SDL_SCANCODE_5,
    ._6 = SDL_SCANCODE_6,
    ._7 = SDL_SCANCODE_7,
    ._8 = SDL_SCANCODE_8,
    ._9 = SDL_SCANCODE_9,
    ._0 = SDL_SCANCODE_0,
    .enter = SDL_SCANCODE_RETURN,
    .escape = SDL_SCANCODE_ESCAPE,
    .backspace = SDL_SCANCODE_BACKSPACE,
    .tab = SDL_SCANCODE_TAB,
    .space = SDL_SCANCODE_SPACE,
    .minus = SDL_SCANCODE_MINUS,
    .equal = SDL_SCANCODE_EQUALS,
    .left_bracket = SDL_SCANCODE_LEFTBRACKET,
    .right_bracket = SDL_SCANCODE_RIGHTBRACKET,
    .backslash = SDL_SCANCODE_BACKSLASH,
    .nonus_hash = SDL_SCANCODE_NONUSHASH,
    .semicolon = SDL_SCANCODE_SEMICOLON,
    .apostrophe = SDL_SCANCODE_APOSTROPHE,
    .grave = SDL_SCANCODE_GRAVE,
    .comma = SDL_SCANCODE_COMMA,
    .period = SDL_SCANCODE_PERIOD,
    .slash = SDL_SCANCODE_SLASH,
    .capslock = SDL_SCANCODE_CAPSLOCK,
    .f1 = SDL_SCANCODE_F1,
    .f2 = SDL_SCANCODE_F2,
    .f3 = SDL_SCANCODE_F3,
    .f4 = SDL_SCANCODE_F4,
    .f5 = SDL_SCANCODE_F5,
    .f6 = SDL_SCANCODE_F6,
    .f7 = SDL_SCANCODE_F7,
    .f8 = SDL_SCANCODE_F8,
    .f9 = SDL_SCANCODE_F9,
    .f10 = SDL_SCANCODE_F10,
    .f11 = SDL_SCANCODE_F11,
    .f12 = SDL_SCANCODE_F12,
    .print_screen = SDL_SCANCODE_PRINTSCREEN,
    .scroll_lock = SDL_SCANCODE_SCROLLLOCK,
    .pause = SDL_SCANCODE_PAUSE,
    .insert = SDL_SCANCODE_INSERT,
    .home = SDL_SCANCODE_HOME,
    .page_up = SDL_SCANCODE_PAGEUP,
    .delete = SDL_SCANCODE_DELETE,
    .end = SDL_SCANCODE_END,
    .page_down = SDL_SCANCODE_PAGEDOWN,
    .right = SDL_SCANCODE_RIGHT,
    .left = SDL_SCANCODE_LEFT,
    .down = SDL_SCANCODE_DOWN,
    .up = SDL_SCANCODE_UP,
    .num_lock = SDL_SCANCODE_NUMLOCKCLEAR,
    .key_pad_divide = SDL_SCANCODE_KP_DIVIDE,
    .key_pad_multiply = SDL_SCANCODE_KP_MULTIPLY,
    .key_pad_minus = SDL_SCANCODE_KP_MINUS,
    .key_pad_plus = SDL_SCANCODE_KP_PLUS,
    .key_pad_enter = SDL_SCANCODE_KP_ENTER,
    .key_pad_1 = SDL_SCANCODE_KP_1,
    .key_pad_2 = SDL_SCANCODE_KP_2,
    .key_pad_3 = SDL_SCANCODE_KP_3,
    .key_pad_4 = SDL_SCANCODE_KP_4,
    .key_pad_5 = SDL_SCANCODE_KP_5,
    .key_pad_6 = SDL_SCANCODE_KP_6,
    .key_pad_7 = SDL_SCANCODE_KP_7,
    .key_pad_8 = SDL_SCANCODE_KP_8,
    .key_pad_9 = SDL_SCANCODE_KP_9,
    .key_pad_0 = SDL_SCANCODE_KP_0,
    .key_pad_period = SDL_SCANCODE_KP_PERIOD,
    .nonus_backslash = SDL_SCANCODE_NONUSBACKSLASH,
    .application = SDL_SCANCODE_APPLICATION,
    .power = SDL_SCANCODE_POWER,
    .key_pad_equals = SDL_SCANCODE_KP_EQUALS,
    .f13 = SDL_SCANCODE_F13,
    .f14 = SDL_SCANCODE_F14,
    .f15 = SDL_SCANCODE_F15,
    .f16 = SDL_SCANCODE_F16,
    .f17 = SDL_SCANCODE_F17,
    .f18 = SDL_SCANCODE_F18,
    .f19 = SDL_SCANCODE_F19,
    .f20 = SDL_SCANCODE_F20,
    .f21 = SDL_SCANCODE_F21,
    .f22 = SDL_SCANCODE_F22,
    .f23 = SDL_SCANCODE_F23,
    .f24 = SDL_SCANCODE_F24,
    .execute = SDL_SCANCODE_EXECUTE,
    .help = SDL_SCANCODE_HELP,
    .menu = SDL_SCANCODE_MENU,
    .select = SDL_SCANCODE_SELECT,
    .stop = SDL_SCANCODE_STOP,
    .again = SDL_SCANCODE_AGAIN,
    .undo = SDL_SCANCODE_UNDO,
    .cut = SDL_SCANCODE_CUT,
    .copy = SDL_SCANCODE_COPY,
    .paste = SDL_SCANCODE_PASTE,
    .find = SDL_SCANCODE_FIND,
    .mute = SDL_SCANCODE_MUTE,
    .volume_up = SDL_SCANCODE_VOLUMEUP,
    .volume_down = SDL_SCANCODE_VOLUMEDOWN,
    .key_pad_comma = SDL_SCANCODE_KP_COMMA,
    .key_pad_equals_as400 = SDL_SCANCODE_KP_EQUALSAS400,
    .alt_erase = SDL_SCANCODE_ALTERASE,
    .sys_req = SDL_SCANCODE_SYSREQ,
    .cancel = SDL_SCANCODE_CANCEL,
    .clear = SDL_SCANCODE_CLEAR,
    .prior = SDL_SCANCODE_PRIOR,
    .return_2 = SDL_SCANCODE_RETURN2,
    .separator = SDL_SCANCODE_SEPARATOR,
    .out = SDL_SCANCODE_OUT,
    .oper = SDL_SCANCODE_OPER,
    .clear_again = SDL_SCANCODE_CLEARAGAIN,
    .cr_sel = SDL_SCANCODE_CRSEL,
    .ex_sel = SDL_SCANCODE_EXSEL,
    .key_pad_00 = SDL_SCANCODE_KP_00,
    .key_pad_000 = SDL_SCANCODE_KP_000,
    .thousands_separator = SDL_SCANCODE_THOUSANDSSEPARATOR,
    .decimal_separator = SDL_SCANCODE_DECIMALSEPARATOR,
    .currency_unit = SDL_SCANCODE_CURRENCYUNIT,
    .currency_subunit = SDL_SCANCODE_CURRENCYSUBUNIT,
    .key_pad_left_paren = SDL_SCANCODE_KP_LEFTPAREN,
    .key_pad_right_paren = SDL_SCANCODE_KP_RIGHTPAREN,
    .key_pad_left_brace = SDL_SCANCODE_KP_LEFTBRACE,
    .key_pad_right_brace = SDL_SCANCODE_KP_RIGHTBRACE,
    .key_pad_tab = SDL_SCANCODE_KP_TAB,
    .key_pad_backspace = SDL_SCANCODE_KP_BACKSPACE,
    .key_pad_a = SDL_SCANCODE_KP_A,
    .key_pad_b = SDL_SCANCODE_KP_B,
    .key_pad_c = SDL_SCANCODE_KP_C,
    .key_pad_d = SDL_SCANCODE_KP_D,
    .key_pad_e = SDL_SCANCODE_KP_E,
    .key_pad_f = SDL_SCANCODE_KP_F,
    .key_pad_xor = SDL_SCANCODE_KP_XOR,
    .key_pad_power = SDL_SCANCODE_KP_POWER,
    .key_pad_percent = SDL_SCANCODE_KP_PERCENT,
    .key_pad_less = SDL_SCANCODE_KP_LESS,
    .key_pad_greater = SDL_SCANCODE_KP_GREATER,
    .key_pad_ampersand = SDL_SCANCODE_KP_AMPERSAND,
    .key_pad_double_ampersand = SDL_SCANCODE_KP_DBLAMPERSAND,
    .key_pad_vertical_bar = SDL_SCANCODE_KP_VERTICALBAR,
    .key_pad_double_vertical_bar = SDL_SCANCODE_KP_DBLVERTICALBAR,
    .key_pad_colon = SDL_SCANCODE_KP_COLON,
    .key_pad_hash = SDL_SCANCODE_KP_HASH,
    .key_pad_space = SDL_SCANCODE_KP_SPACE,
    .key_pad_at = SDL_SCANCODE_KP_AT,
    .key_pad_exclamation = SDL_SCANCODE_KP_EXCLAM,
    .key_pad_mem_store = SDL_SCANCODE_KP_MEMSTORE,
    .key_pad_mem_recall = SDL_SCANCODE_KP_MEMRECALL,
    .key_pad_mem_clear = SDL_SCANCODE_KP_MEMCLEAR,
    .key_pad_mem_add = SDL_SCANCODE_KP_MEMADD,
    .key_pad_mem_subtract = SDL_SCANCODE_KP_MEMSUBTRACT,
    .key_pad_mem_multiply = SDL_SCANCODE_KP_MEMMULTIPLY,
    .key_pad_mem_divide = SDL_SCANCODE_KP_MEMDIVIDE,
    .key_pad_plus_minus = SDL_SCANCODE_KP_PLUSMINUS,
    .key_pad_clear = SDL_SCANCODE_KP_CLEAR,
    .key_pad_clear_entry = SDL_SCANCODE_KP_CLEARENTRY,
    .key_pad_binary = SDL_SCANCODE_KP_BINARY,
    .key_pad_octal = SDL_SCANCODE_KP_OCTAL,
    .key_pad_decimal = SDL_SCANCODE_KP_DECIMAL,
    .key_pad_hexadecimal = SDL_SCANCODE_KP_HEXADECIMAL,
    .left_control = SDL_SCANCODE_LCTRL,
    .left_shift = SDL_SCANCODE_LSHIFT,
    .left_alt = SDL_SCANCODE_LALT,
    .left_gui = SDL_SCANCODE_LGUI,
    .right_control = SDL_SCANCODE_RCTRL,
    .right_shift = SDL_SCANCODE_RSHIFT,
    .right_alt = SDL_SCANCODE_RALT,
    .right_gui = SDL_SCANCODE_RGUI,
    .mode = SDL_SCANCODE_MODE,
    .audio_next = SDL_SCANCODE_AUDIONEXT,
    .audio_previous = SDL_SCANCODE_AUDIOPREV,
    .audio_stop = SDL_SCANCODE_AUDIOSTOP,
    .audio_play = SDL_SCANCODE_AUDIOPLAY,
    .audio_mute = SDL_SCANCODE_AUDIOMUTE,
    .media_select = SDL_SCANCODE_MEDIASELECT,
    .www = SDL_SCANCODE_WWW,
    .mail = SDL_SCANCODE_MAIL,
    .calculator = SDL_SCANCODE_CALCULATOR,
    .computer = SDL_SCANCODE_COMPUTER,
    .ac_search = SDL_SCANCODE_AC_SEARCH,
    .ac_home = SDL_SCANCODE_AC_HOME,
    .ac_back = SDL_SCANCODE_AC_BACK,
    .ac_forward = SDL_SCANCODE_AC_FORWARD,
    .ac_stop = SDL_SCANCODE_AC_STOP,
    .ac_refresh = SDL_SCANCODE_AC_REFRESH,
    .ac_bookmarks = SDL_SCANCODE_AC_BOOKMARKS,
    .brightness_down = SDL_SCANCODE_BRIGHTNESSDOWN,
    .brightness_up = SDL_SCANCODE_BRIGHTNESSUP,
    .display_switch = SDL_SCANCODE_DISPLAYSWITCH,
    .keyboard_illumination_toggle = SDL_SCANCODE_KBDILLUMTOGGLE,
    .keyboard_illumination_down = SDL_SCANCODE_KBDILLUMDOWN,
    .keyboard_illumination_up = SDL_SCANCODE_KBDILLUMUP,
    .eject = SDL_SCANCODE_EJECT,
    .sleep = SDL_SCANCODE_SLEEP,
    .app_1 = SDL_SCANCODE_APP1,
    .app_2 = SDL_SCANCODE_APP2,
    .audio_rewind = SDL_SCANCODE_AUDIOREWIND,
    .audio_fast_forward = SDL_SCANCODE_AUDIOFASTFORWARD,
    .soft_left = SDL_SCANCODE_SOFTLEFT,
    .soft_right = SDL_SCANCODE_SOFTRIGHT,
    .call = SDL_SCANCODE_CALL,
    .end_call = SDL_SCANCODE_ENDCALL,
};

static struct kan_key_modifiers_table_t key_modifiers_table = {
    .left_shift = SDL_KMOD_LSHIFT,
    .right_shift = SDL_KMOD_RSHIFT,
    .left_control = SDL_KMOD_LCTRL,
    .right_control = SDL_KMOD_RCTRL,
    .left_alt = SDL_KMOD_LALT,
    .right_alt = SDL_KMOD_RALT,
    .left_gui = SDL_KMOD_LGUI,
    .right_gui = SDL_KMOD_RGUI,
    .num_lock = SDL_KMOD_NUM,
    .caps_lock = SDL_KMOD_CAPS,
    .mode = SDL_KMOD_MODE,
    .scroll_lock = SDL_KMOD_SCROLL,
};

const struct kan_scan_code_table_t *kan_platform_get_scan_code_table (void)
{
    return &scan_code_table;
}

const struct kan_key_modifiers_table_t *kan_platform_get_key_modifiers_table (void)
{
    return &key_modifiers_table;
}
