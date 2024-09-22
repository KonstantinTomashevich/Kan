#include <kan/api_common/mute_warnings.h>

KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
KAN_MUTE_THIRD_PARTY_WARNINGS_END

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/platform/application.h>
#include <kan/platform/sdl_allocation_adapter.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (platform_application);

static kan_allocation_group_t application_allocation_group;
static kan_allocation_group_t application_events_allocation_group;
static kan_allocation_group_t application_clipboard_allocation_group;

static struct kan_atomic_int_t vulkan_library_requests;

static inline kan_pixel_format_t convert_pixel_format (uint32_t sdl_format)
{
    return (kan_pixel_format_t) sdl_format;
}

static inline uint32_t window_flags_to_sdl_flags (enum kan_platform_window_flag_t flags)
{
    uint32_t sdl_flags = 0u;
    if (flags & KAN_PLATFORM_WINDOW_FLAG_FULLSCREEN)
    {
        sdl_flags |= SDL_WINDOW_FULLSCREEN;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_OPEN_GL)
    {
        sdl_flags |= SDL_WINDOW_OPENGL;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_VULKAN)
    {
        sdl_flags |= SDL_WINDOW_VULKAN;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_METAL)
    {
        sdl_flags |= SDL_WINDOW_METAL;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_OCCLUDED)
    {
        sdl_flags |= SDL_WINDOW_OCCLUDED;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_HIDDEN)
    {
        sdl_flags |= SDL_WINDOW_HIDDEN;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_BORDERLESS)
    {
        sdl_flags |= SDL_WINDOW_BORDERLESS;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_TRANSPARENT)
    {
        sdl_flags |= SDL_WINDOW_TRANSPARENT;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_RESIZABLE)
    {
        sdl_flags |= SDL_WINDOW_RESIZABLE;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_MINIMIZED)
    {
        sdl_flags |= SDL_WINDOW_MINIMIZED;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_MAXIMIZED)
    {
        sdl_flags |= SDL_WINDOW_MAXIMIZED;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_UTILITY)
    {
        sdl_flags |= SDL_WINDOW_UTILITY;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_TOOLTIP)
    {
        sdl_flags |= SDL_WINDOW_TOOLTIP;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_POPUP)
    {
        sdl_flags |= SDL_WINDOW_POPUP_MENU;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_ALWAYS_ON_TOP)
    {
        sdl_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_HIGH_PIXEL_DENSITY)
    {
        sdl_flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_MOUSE_GRABBED)
    {
        sdl_flags |= SDL_WINDOW_MOUSE_GRABBED;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_KEYBOARD_GRABBED)
    {
        sdl_flags |= SDL_WINDOW_KEYBOARD_GRABBED;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_INPUT_FOCUS)
    {
        sdl_flags |= SDL_WINDOW_INPUT_FOCUS;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_MOUSE_FOCUS)
    {
        sdl_flags |= SDL_WINDOW_MOUSE_FOCUS;
    }

    if (flags & KAN_PLATFORM_WINDOW_FLAG_MOUSE_CAPTURE)
    {
        sdl_flags |= SDL_WINDOW_MOUSE_CAPTURE;
    }

    return sdl_flags;
}

static inline enum kan_platform_window_flag_t sdl_flags_to_window_flags (uint64_t sdl_flags)
{
    enum kan_platform_window_flag_t flags = 0u;
    if (sdl_flags & SDL_WINDOW_FULLSCREEN)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_FULLSCREEN;
    }

    if (sdl_flags & SDL_WINDOW_OPENGL)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_OPEN_GL;
    }

    if (sdl_flags & SDL_WINDOW_VULKAN)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_VULKAN;
    }

    if (sdl_flags & SDL_WINDOW_METAL)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_METAL;
    }

    if (sdl_flags & SDL_WINDOW_OCCLUDED)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_OCCLUDED;
    }

    if (sdl_flags & SDL_WINDOW_HIDDEN)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_HIDDEN;
    }

    if (sdl_flags & SDL_WINDOW_BORDERLESS)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_BORDERLESS;
    }

    if (sdl_flags & SDL_WINDOW_TRANSPARENT)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_TRANSPARENT;
    }

    if (sdl_flags & SDL_WINDOW_RESIZABLE)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_RESIZABLE;
    }

    if (sdl_flags & SDL_WINDOW_MINIMIZED)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_MINIMIZED;
    }

    if (sdl_flags & SDL_WINDOW_MAXIMIZED)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_MAXIMIZED;
    }

    if (sdl_flags & SDL_WINDOW_UTILITY)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_UTILITY;
    }

    if (sdl_flags & SDL_WINDOW_TOOLTIP)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_TOOLTIP;
    }

    if (sdl_flags & SDL_WINDOW_POPUP_MENU)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_POPUP;
    }

    if (sdl_flags & SDL_WINDOW_ALWAYS_ON_TOP)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_ALWAYS_ON_TOP;
    }

    if (sdl_flags & SDL_WINDOW_HIGH_PIXEL_DENSITY)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_HIGH_PIXEL_DENSITY;
    }

    if (sdl_flags & SDL_WINDOW_MOUSE_GRABBED)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_MOUSE_GRABBED;
    }

    if (sdl_flags & SDL_WINDOW_KEYBOARD_GRABBED)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_KEYBOARD_GRABBED;
    }

    if (sdl_flags & SDL_WINDOW_INPUT_FOCUS)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_INPUT_FOCUS;
    }

    if (sdl_flags & SDL_WINDOW_MOUSE_FOCUS)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_MOUSE_FOCUS;
    }

    if (sdl_flags & SDL_WINDOW_MOUSE_CAPTURE)
    {
        flags |= KAN_PLATFORM_WINDOW_FLAG_MOUSE_CAPTURE;
    }

    return flags;
}

static inline uint8_t convert_mouse_state (uint32_t sdl_state)
{
    return (uint8_t) sdl_state;
}

void kan_platform_application_event_init (struct kan_platform_application_event_t *instance)
{
    instance->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_QUIT;
    instance->time_ns = 0u;
}

void kan_platform_application_event_move (struct kan_platform_application_event_t *from,
                                          struct kan_platform_application_event_t *to)
{
    to->type = from->type;
    to->time_ns = from->time_ns;

    switch (from->type)
    {
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_QUIT:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_TERMINATING:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_LOW_MEMORY:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_ENTER_BACKGROUND:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_ENTER_FOREGROUND:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_LOCALE_CHANGED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_SYSTEM_THEME_CHANGED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_KEYMAP_CHANGED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_CLIPBOARD_UPDATE:
        break;

    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_ORIENTATION_CHANGED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_ADDED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_REMOVED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_MOVED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_CONTENT_SCALE_CHANGED:
        to->display = from->display;
        break;

    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_SHOWN:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_HIDDEN:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_EXPOSED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MOVED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_RESIZED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_PIXEL_SIZE_CHANGED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MINIMIZED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MAXIMIZED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_RESTORED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MOUSE_ENTER:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MOUSE_LEAVE:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_FOCUS_GAINED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_FOCUS_LOST:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_CLOSE_REQUESTED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_DISPLAY_CHANGED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_DISPLAY_SCALE_CHANGED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_OCCLUDED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_ENTER_FULLSCREEN:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_LEAVE_FULLSCREEN:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_DESTROYED:
        to->window = from->window;
        break;

    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_KEY_DOWN:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_KEY_UP:
        to->keyboard = from->keyboard;
        break;

    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_TEXT_EDITING:
        to->text_editing = from->text_editing;
        break;

    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_TEXT_INPUT:
        to->text_input = from->text_input;
        break;

    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_MOUSE_MOTION:
        to->mouse_motion = from->mouse_motion;
        break;

    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_MOUSE_BUTTON_DOWN:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_MOUSE_BUTTON_UP:
        to->mouse_button = from->mouse_button;
        break;

    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_MOUSE_WHEEL:
        to->mouse_wheel = from->mouse_wheel;
        break;
    }
}

void kan_platform_application_event_shutdown (struct kan_platform_application_event_t *instance)
{
    switch (instance->type)
    {
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_QUIT:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_TERMINATING:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_LOW_MEMORY:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_ENTER_BACKGROUND:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_ENTER_FOREGROUND:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_LOCALE_CHANGED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_SYSTEM_THEME_CHANGED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_ORIENTATION_CHANGED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_ADDED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_REMOVED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_MOVED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_CONTENT_SCALE_CHANGED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_SHOWN:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_HIDDEN:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_EXPOSED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MOVED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_RESIZED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_PIXEL_SIZE_CHANGED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MINIMIZED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MAXIMIZED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_RESTORED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MOUSE_ENTER:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MOUSE_LEAVE:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_FOCUS_GAINED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_FOCUS_LOST:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_CLOSE_REQUESTED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_DISPLAY_CHANGED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_DISPLAY_SCALE_CHANGED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_OCCLUDED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_ENTER_FULLSCREEN:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_LEAVE_FULLSCREEN:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_DESTROYED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_KEY_DOWN:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_KEY_UP:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_KEYMAP_CHANGED:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_MOUSE_MOTION:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_MOUSE_BUTTON_DOWN:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_MOUSE_BUTTON_UP:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_MOUSE_WHEEL:
    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_CLIPBOARD_UPDATE:
        break;

    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_TEXT_EDITING:
        if (instance->text_editing.text)
        {
            kan_free_general (application_events_allocation_group, instance->text_editing.text,
                              strlen (instance->text_editing.text) + 1u);
        }

        break;

    case KAN_PLATFORM_APPLICATION_EVENT_TYPE_TEXT_INPUT:
        if (instance->text_input.text)
        {
            kan_free_general (application_events_allocation_group, instance->text_input.text,
                              strlen (instance->text_input.text) + 1u);
        }

        break;
    }
}

kan_bool_t kan_platform_application_init (void)
{
    ensure_sdl_allocation_adapter_installed ();
    KAN_ASSERT (!SDL_WasInit (SDL_INIT_VIDEO | SDL_INIT_EVENTS))

    if (SDL_InitSubSystem (SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        KAN_LOG (platform_application, KAN_LOG_CRITICAL_ERROR, "Failed to initialize SDL backend for application.")
        return KAN_FALSE;
    }

    application_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "platform_application");
    application_events_allocation_group = kan_allocation_group_get_child (application_allocation_group, "events");
    application_clipboard_allocation_group = kan_allocation_group_get_child (application_allocation_group, "clipboard");

    vulkan_library_requests = kan_atomic_int_init (0);
    return KAN_TRUE;
}

void kan_platform_application_shutdown (void)
{
    KAN_ASSERT (SDL_WasInit (SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    SDL_QuitSubSystem (SDL_INIT_VIDEO | SDL_INIT_EVENTS);
}

kan_bool_t kan_platform_application_fetch_next_event (struct kan_platform_application_event_t *output)
{
    SDL_Event event;
    if (SDL_PollEvent (&event))
    {
        switch (event.type)
        {
        case SDL_EVENT_QUIT:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_QUIT;
            output->time_ns = event.common.timestamp;
            return KAN_TRUE;

        case SDL_EVENT_TERMINATING:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_TERMINATING;
            output->time_ns = event.common.timestamp;
            return KAN_TRUE;

        case SDL_EVENT_LOW_MEMORY:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_LOW_MEMORY;
            output->time_ns = event.common.timestamp;
            return KAN_TRUE;

        case SDL_EVENT_DID_ENTER_BACKGROUND:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_ENTER_BACKGROUND;
            output->time_ns = event.common.timestamp;
            return KAN_TRUE;

        case SDL_EVENT_DID_ENTER_FOREGROUND:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_ENTER_FOREGROUND;
            output->time_ns = event.common.timestamp;
            return KAN_TRUE;

        case SDL_EVENT_LOCALE_CHANGED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_LOCALE_CHANGED;
            output->time_ns = event.common.timestamp;
            return KAN_TRUE;

        case SDL_EVENT_SYSTEM_THEME_CHANGED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_SYSTEM_THEME_CHANGED;
            output->time_ns = event.common.timestamp;
            return KAN_TRUE;

        case SDL_EVENT_DISPLAY_ORIENTATION:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_ORIENTATION_CHANGED;
            output->time_ns = event.common.timestamp;
            output->display.id = (kan_platform_display_id_t) event.display.displayID;
            return KAN_TRUE;

        case SDL_EVENT_DISPLAY_ADDED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_ADDED;
            output->time_ns = event.common.timestamp;
            output->display.id = (kan_platform_display_id_t) event.display.displayID;
            return KAN_TRUE;

        case SDL_EVENT_DISPLAY_REMOVED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_REMOVED;
            output->time_ns = event.common.timestamp;
            output->display.id = (kan_platform_display_id_t) event.display.displayID;
            return KAN_TRUE;

        case SDL_EVENT_DISPLAY_MOVED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_MOVED;
            output->time_ns = event.common.timestamp;
            output->display.id = (kan_platform_display_id_t) event.display.displayID;
            return KAN_TRUE;

        case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_CONTENT_SCALE_CHANGED;
            output->time_ns = event.common.timestamp;
            output->display.id = (kan_platform_display_id_t) event.display.displayID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_SHOWN:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_SHOWN;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_HIDDEN:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_HIDDEN;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_EXPOSED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_EXPOSED;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_MOVED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MOVED;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_RESIZED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_RESIZED;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_PIXEL_SIZE_CHANGED;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_MINIMIZED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MINIMIZED;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_MAXIMIZED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MAXIMIZED;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_RESTORED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_RESTORED;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_MOUSE_ENTER:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MOUSE_ENTER;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MOUSE_LEAVE;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_FOCUS_GAINED;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_FOCUS_LOST;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_CLOSE_REQUESTED;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_DISPLAY_CHANGED;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_DISPLAY_SCALE_CHANGED;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_OCCLUDED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_OCCLUDED;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_ENTER_FULLSCREEN;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_LEAVE_FULLSCREEN;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_WINDOW_DESTROYED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_DESTROYED;
            output->time_ns = event.common.timestamp;
            output->window.id = (kan_platform_window_id_t) event.window.windowID;
            return KAN_TRUE;

        case SDL_EVENT_KEY_DOWN:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_KEY_DOWN;
            output->time_ns = event.common.timestamp;
            output->keyboard.window_id = (kan_platform_window_id_t) event.key.windowID;
            output->keyboard.repeat = event.key.repeat > 0 ? KAN_TRUE : KAN_FALSE;
            output->keyboard.scan_code = (kan_scan_code_t) event.key.keysym.scancode;
            output->keyboard.key_code = (kan_key_code_t) event.key.keysym.sym;
            output->keyboard.modifiers = (kan_key_modifier_mask_t) event.key.keysym.mod;
            return KAN_TRUE;

        case SDL_EVENT_KEY_UP:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_KEY_DOWN;
            output->time_ns = event.common.timestamp;
            output->keyboard.window_id = (kan_platform_window_id_t) event.key.windowID;
            output->keyboard.repeat = event.key.repeat > 0 ? KAN_TRUE : KAN_FALSE;
            output->keyboard.scan_code = (kan_scan_code_t) event.key.keysym.scancode;
            output->keyboard.key_code = (kan_key_code_t) event.key.keysym.sym;
            output->keyboard.modifiers = (kan_key_modifier_mask_t) event.key.keysym.mod;
            return KAN_TRUE;

        case SDL_EVENT_TEXT_EDITING:
        {
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_TEXT_EDITING;
            output->time_ns = event.common.timestamp;
            output->text_editing.window_id = event.edit.windowID;

            const uint64_t text_length = strlen (event.edit.text);
            output->text_editing.text =
                kan_allocate_general (application_events_allocation_group, text_length + 1u, _Alignof (char));
            memcpy (output->text_editing.text, event.edit.text, text_length + 1u);

            output->text_editing.cursor_start = event.edit.start;
            output->text_editing.cursor_length = event.edit.length;
            return KAN_TRUE;
        }

        case SDL_EVENT_TEXT_INPUT:
        {
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_TEXT_INPUT;
            output->time_ns = event.common.timestamp;
            output->text_input.window_id = event.text.windowID;

            const uint64_t text_length = strlen (event.text.text);
            output->text_input.text =
                kan_allocate_general (application_events_allocation_group, text_length + 1u, _Alignof (char));
            memcpy (output->text_input.text, event.text.text, text_length + 1u);
            return KAN_TRUE;
        }

        case SDL_EVENT_KEYMAP_CHANGED:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_KEYMAP_CHANGED;
            output->time_ns = event.common.timestamp;
            return KAN_TRUE;

        case SDL_EVENT_MOUSE_MOTION:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_MOUSE_MOTION;
            output->time_ns = event.common.timestamp;
            output->mouse_motion.window_id = (kan_platform_window_id_t) event.motion.windowID;
            output->mouse_motion.button_state = (uint8_t) event.motion.state;
            output->mouse_motion.window_x = event.motion.x;
            output->mouse_motion.window_y = event.motion.y;
            output->mouse_motion.window_x_relative = event.motion.xrel;
            output->mouse_motion.window_y_relative = event.motion.yrel;
            return KAN_TRUE;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_MOUSE_BUTTON_DOWN;
            output->time_ns = event.common.timestamp;
            output->mouse_button.window_id = (kan_platform_window_id_t) event.button.windowID;
            output->mouse_button.button = (kan_mouse_button_t) event.button.button;
            output->mouse_button.clicks = event.button.clicks;
            output->mouse_button.window_x = event.button.x;
            output->mouse_button.window_y = event.button.y;
            return KAN_TRUE;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_MOUSE_BUTTON_UP;
            output->time_ns = event.common.timestamp;
            output->mouse_button.window_id = (kan_platform_window_id_t) event.button.windowID;
            output->mouse_button.button = (kan_mouse_button_t) event.button.button;
            output->mouse_button.clicks = event.button.clicks;
            output->mouse_button.window_x = event.button.x;
            output->mouse_button.window_y = event.button.y;
            return KAN_TRUE;

        case SDL_EVENT_MOUSE_WHEEL:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_MOUSE_WHEEL;
            output->time_ns = event.common.timestamp;
            output->mouse_wheel.window_id = (kan_platform_window_id_t) event.wheel.windowID;
            output->mouse_wheel.wheel_x = event.wheel.x;
            output->mouse_wheel.wheel_y = event.wheel.y;
            output->mouse_wheel.window_x = event.wheel.x;
            output->mouse_wheel.window_y = event.wheel.y;
            return KAN_TRUE;

        case SDL_EVENT_CLIPBOARD_UPDATE:
            output->type = KAN_PLATFORM_APPLICATION_EVENT_TYPE_CLIPBOARD_UPDATE;
            output->time_ns = event.common.timestamp;
            return KAN_TRUE;

        default:
            // Unsupported event: skip and go to the next right away.
            return kan_platform_application_fetch_next_event (output);
        }
    }

    return KAN_FALSE;
}

enum kan_platform_system_theme_t kan_platform_application_get_system_theme (void)
{
    switch (SDL_GetSystemTheme ())
    {
    case SDL_SYSTEM_THEME_UNKNOWN:
        return KAN_PLATFORM_SYSTEM_THEME_UNKNOWN;

    case SDL_SYSTEM_THEME_LIGHT:
        return KAN_PLATFORM_SYSTEM_THEME_LIGHT;

    case SDL_SYSTEM_THEME_DARK:
        return KAN_PLATFORM_SYSTEM_THEME_DARK;
    }

    return KAN_PLATFORM_SYSTEM_THEME_UNKNOWN;
}

void kan_platform_application_get_display_ids (struct kan_dynamic_array_t *output_ids_array)
{
    KAN_ASSERT (output_ids_array->item_size == sizeof (kan_platform_display_id_t))
    KAN_ASSERT (output_ids_array->size == 0u)

    int count_output = 0;
    SDL_DisplayID *displays = SDL_GetDisplays (&count_output);

    if (displays && count_output > 0)
    {
        kan_dynamic_array_set_capacity (output_ids_array, (uint64_t) count_output);
        for (int index = 0; index < count_output; ++index)
        {
            *(kan_platform_display_id_t *) kan_dynamic_array_add_last (output_ids_array) =
                (kan_platform_display_id_t) displays[index];
        }

        SDL_free (displays);
    }
    else
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Failed to get display ids, backend error: %s", SDL_GetError ())
    }
}

kan_platform_display_id_t kan_platform_application_get_primary_display_id (void)
{
    SDL_DisplayID id = SDL_GetPrimaryDisplay ();
    if (id == 0)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Failed to get primary display id, backend error: %s",
                 SDL_GetError ())
        return KAN_INVALID_PLATFORM_DISPLAY_ID;
    }

    return (kan_platform_display_id_t) id;
}

kan_bool_t kan_platform_application_get_display_bounds (kan_platform_display_id_t display_id,
                                                        struct kan_platform_integer_bounds_t *output_bounds)
{
    SDL_Rect bounds;
    if (SDL_GetDisplayUsableBounds ((SDL_DisplayID) display_id, &bounds) != 0)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Failed to get display %llu bounds, backend error: %s",
                 (unsigned long long) display_id, SDL_GetError ())
        return KAN_FALSE;
    }

    output_bounds->min_x = (int32_t) bounds.x;
    output_bounds->min_y = (int32_t) bounds.y;
    output_bounds->max_x = (int32_t) (bounds.x + bounds.w);
    output_bounds->max_y = (int32_t) (bounds.y + bounds.h);
    return KAN_TRUE;
}

enum kan_platform_display_orientation_t kan_platform_application_get_display_orientation (
    kan_platform_display_id_t display_id)
{
    switch (SDL_GetCurrentDisplayOrientation ((SDL_DisplayID) display_id))
    {
    case SDL_ORIENTATION_UNKNOWN:
        return KAN_PLATFORM_DISPLAY_ORIENTATION_UNKNOWN;

    case SDL_ORIENTATION_LANDSCAPE:
        return KAN_PLATFORM_DISPLAY_ORIENTATION_LANDSCAPE;

    case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
        return KAN_PLATFORM_DISPLAY_ORIENTATION_LANDSCAPE_FLIPPED;

    case SDL_ORIENTATION_PORTRAIT:
        return KAN_PLATFORM_DISPLAY_ORIENTATION_PORTRAIT;

    case SDL_ORIENTATION_PORTRAIT_FLIPPED:
        return KAN_PLATFORM_DISPLAY_ORIENTATION_PORTRAIT_FLIPPED;
    }

    return KAN_PLATFORM_DISPLAY_ORIENTATION_UNKNOWN;
}

float kan_platform_application_get_display_content_scale (kan_platform_display_id_t display_id)
{
    const float content_scale = SDL_GetDisplayContentScale ((SDL_DisplayID) display_id);
    return content_scale == 0.0f ? 1.0f : content_scale;
}

void kan_platform_application_get_fullscreen_display_modes (kan_platform_display_id_t display_id,
                                                            struct kan_dynamic_array_t *output_array)
{
    KAN_ASSERT (output_array->item_size == sizeof (struct kan_platform_display_mode_t))
    KAN_ASSERT (output_array->size == 0u)

    int count_output = 0;
    const SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes ((SDL_DisplayID) display_id, &count_output);

    if (modes && count_output > 0)
    {
        kan_dynamic_array_set_capacity (output_array, (uint64_t) count_output);
        for (int index = 0; index < count_output; ++index)
        {
            *(struct kan_platform_display_mode_t *) kan_dynamic_array_add_last (output_array) =
                (struct kan_platform_display_mode_t) {
                    .pixel_format = convert_pixel_format (modes[index]->format),
                    .width = (uint32_t) modes[index]->w,
                    .height = (uint32_t) modes[index]->h,
                    .pixel_density = modes[index]->pixel_density,
                    .refresh_rate = modes[index]->refresh_rate,
                };
        }

        SDL_free (modes);
    }
    else
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Failed to get display %llu modes, backend error: %s",
                 (unsigned long long) display_id, SDL_GetError ())
    }
}

kan_bool_t kan_platform_application_get_current_display_mode (kan_platform_display_id_t display_id,
                                                              struct kan_platform_display_mode_t *output)
{
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode ((SDL_DisplayID) display_id);
    if (mode)
    {
        *output = (struct kan_platform_display_mode_t) {
            .pixel_format = convert_pixel_format (mode->format),
            .width = (uint32_t) mode->w,
            .height = (uint32_t) mode->h,
            .pixel_density = mode->pixel_density,
            .refresh_rate = mode->refresh_rate,
        };

        return KAN_TRUE;
    }
    else
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Failed to get display %llu current mode, backend error: %s",
                 (unsigned long long) display_id, SDL_GetError ())
        return KAN_FALSE;
    }
}

kan_bool_t kan_platform_application_get_desktop_display_mode (kan_platform_display_id_t display_id,
                                                              struct kan_platform_display_mode_t *output)
{
    const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode ((SDL_DisplayID) display_id);
    if (mode)
    {
        *output = (struct kan_platform_display_mode_t) {
            .pixel_format = convert_pixel_format (mode->format),
            .width = (uint32_t) mode->w,
            .height = (uint32_t) mode->h,
            .pixel_density = mode->pixel_density,
            .refresh_rate = mode->refresh_rate,
        };

        return KAN_TRUE;
    }
    else
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Failed to get display %llu desktop mode, backend error: %s",
                 (unsigned long long) display_id, SDL_GetError ())
        return KAN_FALSE;
    }
}

kan_platform_window_id_t kan_platform_application_window_create (const char *title,
                                                                 uint32_t width,
                                                                 uint32_t height,
                                                                 enum kan_platform_window_flag_t flags)
{
    SDL_Window *window = SDL_CreateWindow (title, (int) width, (int) height, window_flags_to_sdl_flags (flags));
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Failed to create window, backend error: %s", SDL_GetError ())
        return KAN_INVALID_PLATFORM_WINDOW_ID;
    }

    return (kan_platform_window_id_t) SDL_GetWindowID (window);
}

kan_platform_display_id_t kan_platform_application_window_get_display_id (kan_platform_window_id_t window_id)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_INVALID_PLATFORM_DISPLAY_ID;
    }

    SDL_DisplayID display_id = SDL_GetDisplayForWindow (window);
    if (display_id == 0)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Failed to get display for window %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_INVALID_PLATFORM_DISPLAY_ID;
    }

    return (kan_platform_display_id_t) display_id;
}

float kan_platform_application_window_get_pixel_density (kan_platform_window_id_t window_id)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_INVALID_PLATFORM_DISPLAY_ID;
    }

    const float density = SDL_GetWindowPixelDensity (window);
    return density == 0.0f ? 1.0f : density;
}

float kan_platform_application_window_get_display_scale (kan_platform_window_id_t window_id)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return 1.0f;
    }

    const float scale = SDL_GetWindowDisplayScale (window);
    return scale == 0.0f ? 1.0f : scale;
}

kan_pixel_format_t kan_platform_application_window_get_pixel_format (kan_platform_window_id_t window_id)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_INVALID_PIXEL_FORMAT;
    }

    return (kan_pixel_format_t) SDL_GetWindowPixelFormat (window);
}

kan_bool_t kan_platform_application_window_enter_fullscreen (kan_platform_window_id_t window_id,
                                                             const struct kan_platform_display_mode_t *display_mode)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    const SDL_DisplayMode *closest_mode =
        SDL_GetClosestFullscreenDisplayMode (SDL_GetDisplayForWindow (window), (int) display_mode->width,
                                             (int) display_mode->height, display_mode->refresh_rate, KAN_TRUE);

    if ((uint32_t) closest_mode->format != display_mode->pixel_format || closest_mode->w != (int) display_mode->width ||
        closest_mode->h != (int) display_mode->height ||
        fabs (closest_mode->refresh_rate - display_mode->refresh_rate) > 0.01f ||
        fabs (closest_mode->pixel_density - display_mode->pixel_density) > 0.01f)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR,
                 "Unable to find appropriate display format for showing window with id %llu in fullscreen, backend "
                 "error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    if (SDL_SetWindowFullscreenMode (window, closest_mode) != 0)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR,
                 "Failed to set appropriate display format for showing window with id %llu in fullscreen, backend "
                 "error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_SetWindowFullscreen (window, SDL_TRUE) == 0;
}

kan_bool_t kan_platform_application_window_leave_fullscreen (kan_platform_window_id_t window_id)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_SetWindowFullscreen (window, SDL_FALSE) == 0;
}

enum kan_platform_window_flag_t kan_platform_application_window_get_flags (kan_platform_window_id_t window_id)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return 0u;
    }

    return sdl_flags_to_window_flags (SDL_GetWindowFlags (window));
}

kan_bool_t kan_platform_application_window_set_title (kan_platform_window_id_t window_id, const char *title)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_SetWindowTitle (window, title) == 0;
}

const char *kan_platform_application_window_get_title (kan_platform_window_id_t window_id)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return NULL;
    }

    return SDL_GetWindowTitle (window);
}

kan_bool_t kan_platform_application_window_set_icon (kan_platform_window_id_t window_id,
                                                     kan_pixel_format_t pixel_format,
                                                     uint32_t width,
                                                     uint32_t height,
                                                     const void *data)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    // For now, we're using only 4 byte formats.
    const uint32_t pitch = width * 4u;

    SDL_Surface *icon_surface =
        SDL_CreateSurfaceFrom ((void *) data, (int) width, (int) height, (int) pitch, pixel_format);

    if (!icon_surface)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to create surface from icon data, backend error: %s",
                 SDL_GetError ());
        return KAN_FALSE;
    }

    return SDL_SetWindowIcon (window, icon_surface) == 0;
}

kan_bool_t kan_platform_application_window_set_position (kan_platform_window_id_t window_id, int32_t x, int32_t y)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_SetWindowPosition (window, (int) x, (int) y) == 0;
}

kan_bool_t kan_platform_application_window_get_position (kan_platform_window_id_t window_id,
                                                         int32_t *output_x,
                                                         int32_t *output_y)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    int x;
    int y;

    if (!SDL_GetWindowPosition (window, &x, &y))
    {
        return KAN_FALSE;
    }

    *output_x = (int32_t) x;
    *output_y = (int32_t) y;
    return KAN_TRUE;
}

kan_bool_t kan_platform_application_window_set_size (kan_platform_window_id_t window_id,
                                                     uint32_t width,
                                                     uint32_t height)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_SetWindowSize (window, (int) width, (int) height) == 0;
}

kan_bool_t kan_platform_application_window_get_size (kan_platform_window_id_t window_id,
                                                     uint32_t *output_width,
                                                     uint32_t *output_height)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    int width;
    int height;

    if (!SDL_GetWindowSize (window, &width, &height))
    {
        return KAN_FALSE;
    }

    *output_width = (int32_t) width;
    *output_height = (int32_t) height;
    return KAN_TRUE;
}

kan_bool_t kan_platform_application_window_get_size_for_render (kan_platform_window_id_t window_id,
                                                                uint32_t *output_width,
                                                                uint32_t *output_height)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    int width;
    int height;

    if (!SDL_GetWindowSizeInPixels (window, &width, &height))
    {
        return KAN_FALSE;
    }

    *output_width = (int32_t) width;
    *output_height = (int32_t) height;
    return KAN_TRUE;
}

kan_bool_t kan_platform_application_window_set_minimum_size (kan_platform_window_id_t window_id,
                                                             uint32_t width,
                                                             uint32_t height)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_SetWindowMinimumSize (window, (int) width, (int) height) == 0;
}

kan_bool_t kan_platform_application_window_get_minimum_size (kan_platform_window_id_t window_id,
                                                             uint32_t *output_width,
                                                             uint32_t *output_height)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    int width;
    int height;

    if (!SDL_GetWindowMinimumSize (window, &width, &height))
    {
        return KAN_FALSE;
    }

    *output_width = (int32_t) width;
    *output_height = (int32_t) height;
    return KAN_TRUE;
}

kan_bool_t kan_platform_application_window_set_maximum_size (kan_platform_window_id_t window_id,
                                                             uint32_t width,
                                                             uint32_t height)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_SetWindowMaximumSize (window, (int) width, (int) height) == 0;
}

kan_bool_t kan_platform_application_window_get_maximum_size (kan_platform_window_id_t window_id,
                                                             uint32_t *output_width,
                                                             uint32_t *output_height)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    int width;
    int height;

    if (!SDL_GetWindowMaximumSize (window, &width, &height))
    {
        return KAN_FALSE;
    }

    *output_width = (int32_t) width;
    *output_height = (int32_t) height;
    return KAN_TRUE;
}

kan_bool_t kan_platform_application_window_set_bordered (kan_platform_window_id_t window_id, kan_bool_t bordered)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_SetWindowBordered (window, bordered ? SDL_TRUE : SDL_FALSE) == 0;
}

kan_bool_t kan_platform_application_window_set_resizable (kan_platform_window_id_t window_id, kan_bool_t resizable)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_SetWindowResizable (window, resizable ? SDL_TRUE : SDL_FALSE) == 0;
}

kan_bool_t kan_platform_application_window_set_always_on_top (kan_platform_window_id_t window_id,
                                                              kan_bool_t always_on_top)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_SetWindowAlwaysOnTop (window, always_on_top ? SDL_TRUE : SDL_FALSE) == 0;
}

kan_bool_t kan_platform_application_window_show (kan_platform_window_id_t window_id)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_ShowWindow (window) == 0;
}

kan_bool_t kan_platform_application_window_hide (kan_platform_window_id_t window_id)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_HideWindow (window) == 0;
}

kan_bool_t kan_platform_application_window_raise (kan_platform_window_id_t window_id)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_RaiseWindow (window) == 0;
}

kan_bool_t kan_platform_application_window_minimize (kan_platform_window_id_t window_id)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_MinimizeWindow (window) == 0;
}

kan_bool_t kan_platform_application_window_maximize (kan_platform_window_id_t window_id)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_MaximizeWindow (window) == 0;
}

kan_bool_t kan_platform_application_window_restore (kan_platform_window_id_t window_id)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_RestoreWindow (window) == 0;
}

kan_bool_t kan_platform_application_window_set_mouse_grab (kan_platform_window_id_t window_id, kan_bool_t grab_mouse)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_SetWindowMouseGrab (window, grab_mouse ? SDL_TRUE : SDL_FALSE) == 0;
}

kan_bool_t kan_platform_application_window_set_keyboard_grab (kan_platform_window_id_t window_id,
                                                              kan_bool_t grab_keyboard)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_SetWindowKeyboardGrab (window, grab_keyboard ? SDL_TRUE : SDL_FALSE) == 0;
}

kan_bool_t kan_platform_application_window_set_opacity (kan_platform_window_id_t window_id, float opacity)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return KAN_FALSE;
    }

    return SDL_SetWindowOpacity (window, opacity) == 0;
}

float kan_platform_application_window_get_opacity (kan_platform_window_id_t window_id)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return 0.0f;
    }

    float result;
    if (SDL_GetWindowOpacity (window, &result) != 0)
    {
        return 0.0f;
    }

    return result;
}

void kan_platform_application_window_set_focusable (kan_platform_window_id_t window_id, kan_bool_t focusable)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return;
    }

    SDL_SetWindowFocusable (window, focusable ? SDL_TRUE : SDL_FALSE);
}

_Static_assert (sizeof (VkInstance) <= sizeof (uint64_t), "VkInstance is not bigger than 64 bit integer.");
_Static_assert (sizeof (VkSurfaceKHR) <= sizeof (uint64_t), "VkInstance is not bigger than 64 bit integer.");

#if !defined(VK_NULL_HANDLE)
#    define VK_NULL_HANDLE 0u
#endif

uint64_t kan_platform_application_window_create_vulkan_surface (kan_platform_window_id_t window_id,
                                                                uint64_t vulkan_instance,
                                                                void *vulkan_allocation_callbacks)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return VK_NULL_HANDLE;
    }

    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface (window, (VkInstance) vulkan_instance, vulkan_allocation_callbacks, &surface))
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Failed to create Vulkan surface, backend error: %s",
                 SDL_GetError ())
        return VK_NULL_HANDLE;
    }

    return (uint64_t) surface;
}

void kan_platform_application_window_destroy_vulkan_surface (kan_platform_window_id_t window_id,
                                                             uint64_t vulkan_instance,
                                                             uint64_t vulkan_surface,
                                                             void *vulkan_allocation_callbacks)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return;
    }

    SDL_Vulkan_DestroySurface ((VkInstance) vulkan_instance, (VkSurfaceKHR) vulkan_surface,
                               vulkan_allocation_callbacks);
}

void kan_platform_application_window_destroy (kan_platform_window_id_t window_id)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return;
    }

    SDL_DestroyWindow (window);
}

uint8_t kan_platform_application_get_mouse_state_local_to_focus (float *x, float *y)
{
    const uint32_t sdl_state = SDL_GetMouseState (x, y);
    return convert_mouse_state (sdl_state);
}

uint8_t kan_platform_application_get_global_mouse_state (float *x, float *y)
{
    const uint32_t sdl_state = SDL_GetGlobalMouseState (x, y);
    return convert_mouse_state (sdl_state);
}

void kan_platform_application_warp_mouse_in_window (kan_platform_window_id_t window_id, float x, float y)
{
    SDL_Window *window = SDL_GetWindowFromID ((SDL_WindowID) window_id);
    if (!window)
    {
        KAN_LOG (platform_application, KAN_LOG_ERROR, "Unable to find window with id %llu, backend error: %s",
                 (unsigned long long) window_id, SDL_GetError ())
        return;
    }

    SDL_WarpMouseInWindow (window, x, y);
}

void kan_platform_application_warp_mouse_global (float x, float y)
{
    SDL_WarpMouseGlobal (x, y);
}

void kan_platform_application_set_cursor_visible (kan_bool_t visible)
{
    if (visible)
    {
        SDL_ShowCursor ();
    }
    else
    {
        SDL_HideCursor ();
    }
}

kan_allocation_group_t kan_platform_application_get_clipboard_allocation_group (void)
{
    return application_clipboard_allocation_group;
}

char *kan_platform_application_extract_text_from_clipboard (void)
{
    char *sdl_text = SDL_GetClipboardText ();
    if (!sdl_text)
    {
        return NULL;
    }

    uint64_t text_length = strlen (sdl_text);
    char *kan_text = kan_allocate_general (application_clipboard_allocation_group, text_length + 1u, _Alignof (char));
    memcpy (kan_text, sdl_text, text_length + 1u);

    SDL_free (sdl_text);
    return kan_text;
}

PLATFORM_API void kan_platform_application_put_text_into_clipboard (const char *text)
{
    SDL_SetClipboardText (text);
}

kan_bool_t kan_platform_application_register_vulkan_library_usage (void)
{
    if (kan_atomic_int_add (&vulkan_library_requests, 1) == 0)
    {
        if (SDL_Vulkan_LoadLibrary (NULL) == 0)
        {
            return KAN_TRUE;
        }
        else
        {
            KAN_LOG (platform_application, KAN_LOG_ERROR, "Failed to load Vulkan library using SDL. Error: %s.",
                     SDL_GetError ())
            kan_atomic_int_add (&vulkan_library_requests, -1);
            return KAN_FALSE;
        }
    }

    return KAN_TRUE;
}

void *kan_platform_application_request_vulkan_resolve_function (void)
{
    return (void *) SDL_Vulkan_GetVkGetInstanceProcAddr ();
}

void kan_platform_application_request_vulkan_extensions (struct kan_dynamic_array_t *output,
                                                         kan_allocation_group_t allocation_group)
{
    uint32_t count;
    const char *const *extensions = SDL_Vulkan_GetInstanceExtensions (&count);
    kan_dynamic_array_init (output, (uint64_t) count, sizeof (char *), _Alignof (char *), allocation_group);

    for (uint64_t index = 0u; index < (uint64_t) count; ++index)
    {
        char **extension_output = kan_dynamic_array_add_last (output);
        uint64_t length = strlen (extensions[index]);
        *extension_output = kan_allocate_general (allocation_group, length + 1u, _Alignof (char));
        memcpy (*extension_output, extensions[index], length + 1u);
    }
}

void kan_platform_application_unregister_vulkan_library_usage (void)
{
    if (kan_atomic_int_add (&vulkan_library_requests, -1) == 1)
    {
        SDL_Vulkan_UnloadLibrary ();
    }
}
