#pragma once

#include <kan/api_common/mute_warnings.h>

KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>
KAN_MUTE_THIRD_PARTY_WARNINGS_END

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/platform/application.h>
#include <kan/platform/keyboard.h>
#include <kan/platform/mouse.h>
#include <kan/platform/pixel.h>

enum kan_platform_window_flag_t to_kan_window_flags (uint64_t sdl_flags);
uint32_t to_sdl_window_flags (enum kan_platform_window_flag_t flags);

enum kan_platform_scan_code_t to_kan_scan_code (SDL_Scancode scan_code);
enum kan_platform_modifier_mask_t to_kan_modifier_mask (SDL_Keymod modifiers);
enum kan_platform_mouse_button_t to_kan_mouse_button (uint8_t button);

enum kan_platform_pixel_format_t to_kan_pixel_format (SDL_PixelFormat pixel_format);
enum SDL_PixelFormat to_sdl_pixel_format (enum kan_platform_pixel_format_t pixel_format);
