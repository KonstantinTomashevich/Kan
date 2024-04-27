#pragma once

#include <platform_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Provides basic API for platform-specific keyboard data.

KAN_C_HEADER_BEGIN

/// \brief Scan code describes physical key on keyboard.
typedef uint32_t kan_scan_code_t;

/// \brief Key code describes logical key on keyboard.
typedef uint32_t kan_key_code_t;

/// \brief Describes mask of pressed modifier key.
typedef uint16_t kan_key_modifier_mask_t;

/// \brief Table that lists scan codes for all keys. Key names are based on standard QWERTY layout.
struct kan_scan_code_table_t
{
    kan_scan_code_t a;
    kan_scan_code_t b;
    kan_scan_code_t c;
    kan_scan_code_t d;
    kan_scan_code_t e;
    kan_scan_code_t f;
    kan_scan_code_t g;
    kan_scan_code_t h;
    kan_scan_code_t i;
    kan_scan_code_t j;
    kan_scan_code_t k;
    kan_scan_code_t l;
    kan_scan_code_t m;
    kan_scan_code_t n;
    kan_scan_code_t o;
    kan_scan_code_t p;
    kan_scan_code_t q;
    kan_scan_code_t r;
    kan_scan_code_t s;
    kan_scan_code_t t;
    kan_scan_code_t u;
    kan_scan_code_t v;
    kan_scan_code_t w;
    kan_scan_code_t x;
    kan_scan_code_t y;
    kan_scan_code_t z;

    kan_scan_code_t _1;
    kan_scan_code_t _2;
    kan_scan_code_t _3;
    kan_scan_code_t _4;
    kan_scan_code_t _5;
    kan_scan_code_t _6;
    kan_scan_code_t _7;
    kan_scan_code_t _8;
    kan_scan_code_t _9;
    kan_scan_code_t _0;

    kan_scan_code_t enter;
    kan_scan_code_t escape;
    kan_scan_code_t backspace;
    kan_scan_code_t tab;
    kan_scan_code_t space;

    kan_scan_code_t minus;
    kan_scan_code_t equal;
    kan_scan_code_t left_bracket;
    kan_scan_code_t right_bracket;
    kan_scan_code_t backslash;
    kan_scan_code_t nonus_hash;
    kan_scan_code_t semicolon;
    kan_scan_code_t apostrophe;
    kan_scan_code_t grave;
    kan_scan_code_t comma;
    kan_scan_code_t period;
    kan_scan_code_t slash;

    kan_scan_code_t capslock;

    kan_scan_code_t f1;
    kan_scan_code_t f2;
    kan_scan_code_t f3;
    kan_scan_code_t f4;
    kan_scan_code_t f5;
    kan_scan_code_t f6;
    kan_scan_code_t f7;
    kan_scan_code_t f8;
    kan_scan_code_t f9;
    kan_scan_code_t f10;
    kan_scan_code_t f11;
    kan_scan_code_t f12;

    kan_scan_code_t print_screen;
    kan_scan_code_t scroll_lock;
    kan_scan_code_t pause;
    kan_scan_code_t insert;
    kan_scan_code_t home;
    kan_scan_code_t page_up;
    kan_scan_code_t delete;
    kan_scan_code_t end;
    kan_scan_code_t page_down;
    kan_scan_code_t right;
    kan_scan_code_t left;
    kan_scan_code_t down;
    kan_scan_code_t up;

    kan_scan_code_t num_lock;
    kan_scan_code_t key_pad_divide;
    kan_scan_code_t key_pad_multiply;
    kan_scan_code_t key_pad_minus;
    kan_scan_code_t key_pad_plus;
    kan_scan_code_t key_pad_enter;

    kan_scan_code_t key_pad_1;
    kan_scan_code_t key_pad_2;
    kan_scan_code_t key_pad_3;
    kan_scan_code_t key_pad_4;
    kan_scan_code_t key_pad_5;
    kan_scan_code_t key_pad_6;
    kan_scan_code_t key_pad_7;
    kan_scan_code_t key_pad_8;
    kan_scan_code_t key_pad_9;
    kan_scan_code_t key_pad_0;
    kan_scan_code_t key_pad_period;

    kan_scan_code_t nonus_backslash;
    kan_scan_code_t application;
    kan_scan_code_t power;
    kan_scan_code_t key_pad_equals;
    kan_scan_code_t f13;
    kan_scan_code_t f14;
    kan_scan_code_t f15;
    kan_scan_code_t f16;
    kan_scan_code_t f17;
    kan_scan_code_t f18;
    kan_scan_code_t f19;
    kan_scan_code_t f20;
    kan_scan_code_t f21;
    kan_scan_code_t f22;
    kan_scan_code_t f23;
    kan_scan_code_t f24;
    kan_scan_code_t execute;
    kan_scan_code_t help;
    kan_scan_code_t menu;
    kan_scan_code_t select;
    kan_scan_code_t stop;
    kan_scan_code_t again;
    kan_scan_code_t undo;
    kan_scan_code_t cut;
    kan_scan_code_t copy;
    kan_scan_code_t paste;
    kan_scan_code_t find;
    kan_scan_code_t mute;
    kan_scan_code_t volume_up;
    kan_scan_code_t volume_down;
    kan_scan_code_t key_pad_comma;
    kan_scan_code_t key_pad_equals_as400;

    kan_scan_code_t international_1;
    kan_scan_code_t international_2;
    kan_scan_code_t international_3;
    kan_scan_code_t international_4;
    kan_scan_code_t international_5;
    kan_scan_code_t international_6;
    kan_scan_code_t international_7;
    kan_scan_code_t international_8;
    kan_scan_code_t international_9;

    kan_scan_code_t lang_1;
    kan_scan_code_t lang_2;
    kan_scan_code_t lang_3;
    kan_scan_code_t lang_4;
    kan_scan_code_t lang_5;
    kan_scan_code_t lang_6;
    kan_scan_code_t lang_7;
    kan_scan_code_t lang_8;
    kan_scan_code_t lang_9;

    kan_scan_code_t alt_erase;
    kan_scan_code_t sys_req;
    kan_scan_code_t cancel;
    kan_scan_code_t clear;
    kan_scan_code_t prior;
    kan_scan_code_t return_2;
    kan_scan_code_t separator;
    kan_scan_code_t out;
    kan_scan_code_t oper;
    kan_scan_code_t clear_again;

    kan_scan_code_t cr_sel;
    kan_scan_code_t ex_sel;

    kan_scan_code_t key_pad_00;
    kan_scan_code_t key_pad_000;
    kan_scan_code_t thousands_separator;
    kan_scan_code_t decimal_separator;
    kan_scan_code_t currency_unit;
    kan_scan_code_t currency_subunit;
    kan_scan_code_t key_pad_left_paren;
    kan_scan_code_t key_pad_right_paren;
    kan_scan_code_t key_pad_left_brace;
    kan_scan_code_t key_pad_right_brace;
    kan_scan_code_t key_pad_tab;
    kan_scan_code_t key_pad_backspace;
    kan_scan_code_t key_pad_a;
    kan_scan_code_t key_pad_b;
    kan_scan_code_t key_pad_c;
    kan_scan_code_t key_pad_d;
    kan_scan_code_t key_pad_e;
    kan_scan_code_t key_pad_f;

    kan_scan_code_t key_pad_xor;
    kan_scan_code_t key_pad_power;
    kan_scan_code_t key_pad_percent;
    kan_scan_code_t key_pad_less;
    kan_scan_code_t key_pad_greater;
    kan_scan_code_t key_pad_ampersand;
    kan_scan_code_t key_pad_double_ampersand;
    kan_scan_code_t key_pad_vertical_bar;
    kan_scan_code_t key_pad_double_vertical_bar;
    kan_scan_code_t key_pad_colon;
    kan_scan_code_t key_pad_hash;
    kan_scan_code_t key_pad_space;
    kan_scan_code_t key_pad_at;
    kan_scan_code_t key_pad_exclamation;
    kan_scan_code_t key_pad_mem_store;
    kan_scan_code_t key_pad_mem_recall;
    kan_scan_code_t key_pad_mem_clear;
    kan_scan_code_t key_pad_mem_add;
    kan_scan_code_t key_pad_mem_subtract;
    kan_scan_code_t key_pad_mem_multiply;
    kan_scan_code_t key_pad_mem_divide;
    kan_scan_code_t key_pad_plus_minus;
    kan_scan_code_t key_pad_clear;
    kan_scan_code_t key_pad_clear_entry;
    kan_scan_code_t key_pad_binary;
    kan_scan_code_t key_pad_octal;
    kan_scan_code_t key_pad_decimal;
    kan_scan_code_t key_pad_hexadecimal;

    kan_scan_code_t left_control;
    kan_scan_code_t left_shift;
    kan_scan_code_t left_alt;
    kan_scan_code_t left_gui;
    kan_scan_code_t right_control;
    kan_scan_code_t right_shift;
    kan_scan_code_t right_alt;
    kan_scan_code_t right_gui;

    kan_scan_code_t mode;

    kan_scan_code_t audio_next;
    kan_scan_code_t audio_previous;
    kan_scan_code_t audio_stop;
    kan_scan_code_t audio_play;
    kan_scan_code_t audio_mute;
    kan_scan_code_t media_select;
    kan_scan_code_t www;
    kan_scan_code_t mail;
    kan_scan_code_t calculator;
    kan_scan_code_t computer;

    kan_scan_code_t ac_search;
    kan_scan_code_t ac_home;
    kan_scan_code_t ac_back;
    kan_scan_code_t ac_forward;
    kan_scan_code_t ac_stop;
    kan_scan_code_t ac_refresh;
    kan_scan_code_t ac_bookmarks;

    kan_scan_code_t brightness_down;
    kan_scan_code_t brightness_up;
    kan_scan_code_t display_switch;

    kan_scan_code_t keyboard_illumination_toggle;
    kan_scan_code_t keyboard_illumination_down;
    kan_scan_code_t keyboard_illumination_up;

    kan_scan_code_t eject;
    kan_scan_code_t sleep;

    kan_scan_code_t app_1;
    kan_scan_code_t app_2;

    kan_scan_code_t audio_rewind;
    kan_scan_code_t audio_fast_forward;

    kan_scan_code_t soft_left;
    kan_scan_code_t soft_right;

    kan_scan_code_t call;
    kan_scan_code_t end_call;
};

/// \brief Table the lists codes for modifier keys on this platform.
struct kan_key_modifiers_table_t
{
    kan_key_modifier_mask_t left_shift;
    kan_key_modifier_mask_t right_shift;
    kan_key_modifier_mask_t left_control;
    kan_key_modifier_mask_t right_control;
    kan_key_modifier_mask_t left_alt;
    kan_key_modifier_mask_t right_alt;
    kan_key_modifier_mask_t left_gui;
    kan_key_modifier_mask_t right_gui;
    kan_key_modifier_mask_t num_lock;
    kan_key_modifier_mask_t caps_lock;
    kan_key_modifier_mask_t mode;
    kan_key_modifier_mask_t scroll_lock;
};

/// \brief Returns scan codes table for this platform.
PLATFORM_API const struct kan_scan_code_table_t *kan_platform_get_scan_code_table (void);

/// \brief Returns modifier code table for this platform.
PLATFORM_API const struct kan_key_modifiers_table_t *kan_platform_get_key_modifiers_table (void);

KAN_C_HEADER_END
