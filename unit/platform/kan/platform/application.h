#pragma once

#include <platform_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/platform/keyboard.h>
#include <kan/platform/mouse.h>
#include <kan/platform/pixel.h>

/// \file
/// \brief Provides API for common application-related platform features.
///
/// \par Threading
/// \parblock
/// This API is designed to be called from main thread only as it is a requirement for some platforms.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Size type for windows, displays and other visuals.
/// \details Always 32 bit as there is technically no need for 64 bits.
typedef uint32_t kan_platform_visual_size_t;

/// \brief Offset type for windows, displays and other visuals.
/// \details Always 32 bit as there is technically no need for 64 bits.
typedef int32_t kan_platform_visual_offset_t;

/// \brief Lists platform event types.
enum kan_platform_application_event_type_t
{
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_QUIT = 0u,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_TERMINATING,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_LOW_MEMORY,

    KAN_PLATFORM_APPLICATION_EVENT_TYPE_ENTER_BACKGROUND,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_ENTER_FOREGROUND,

    KAN_PLATFORM_APPLICATION_EVENT_TYPE_LOCALE_CHANGED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_SYSTEM_THEME_CHANGED,

    KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_ORIENTATION_CHANGED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_ADDED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_REMOVED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_MOVED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_DISPLAY_CONTENT_SCALE_CHANGED,

    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_SHOWN,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_HIDDEN,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_EXPOSED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MOVED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_RESIZED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_PIXEL_SIZE_CHANGED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MINIMIZED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MAXIMIZED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_RESTORED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MOUSE_ENTER,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_MOUSE_LEAVE,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_FOCUS_GAINED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_FOCUS_LOST,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_CLOSE_REQUESTED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_DISPLAY_CHANGED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_DISPLAY_SCALE_CHANGED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_OCCLUDED,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_ENTER_FULLSCREEN,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_LEAVE_FULLSCREEN,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_DESTROYED,

    KAN_PLATFORM_APPLICATION_EVENT_TYPE_KEY_DOWN,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_KEY_UP,

    KAN_PLATFORM_APPLICATION_EVENT_TYPE_TEXT_EDITING,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_TEXT_INPUT,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_KEYMAP_CHANGED,

    KAN_PLATFORM_APPLICATION_EVENT_TYPE_MOUSE_MOTION,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_MOUSE_BUTTON_DOWN,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_MOUSE_BUTTON_UP,
    KAN_PLATFORM_APPLICATION_EVENT_TYPE_MOUSE_WHEEL,

    KAN_PLATFORM_APPLICATION_EVENT_TYPE_CLIPBOARD_UPDATE,
};

KAN_TYPED_ID_32_DEFINE (kan_platform_display_id_t);

/// \brief Suffix structure for display events.
struct kan_platform_application_event_display_t
{
    kan_platform_display_id_t id;
};

KAN_TYPED_ID_32_DEFINE (kan_platform_window_id_t);

/// \brief Suffix structure for window events.
struct kan_platform_application_event_window_t
{
    kan_platform_window_id_t id;
};

/// \brief Suffix structure for keyboard events.
struct kan_platform_application_event_keyboard_t
{
    kan_platform_window_id_t window_id;
    kan_bool_t repeat;
    enum kan_platform_scan_code_t scan_code;
    kan_platform_key_code_t key_code;
    enum kan_platform_modifier_mask_t modifiers;
};

/// \brief Suffix structure for text editing events.
struct kan_platform_application_event_text_editing_t
{
    kan_platform_window_id_t window_id;
    char *text;
    kan_platform_visual_size_t cursor_start;
    kan_platform_visual_size_t cursor_length;
};

/// \brief Suffix structure for text input events.
struct kan_platform_application_event_text_input_t
{
    kan_platform_window_id_t window_id;
    char *text;
};

/// \brief Suffix structure for mouse motion events.
struct kan_platform_application_event_mouse_motion_t
{
    kan_platform_window_id_t window_id;
    uint8_t button_state;
    float window_x;
    float window_y;
    float window_x_relative;
    float window_y_relative;
};

/// \brief Suffix structure for mouse button events.
struct kan_platform_application_event_mouse_button_t
{
    kan_platform_window_id_t window_id;
    enum kan_platform_mouse_button_t button;
    uint8_t clicks;
    float window_x;
    float window_y;
};

/// \brief Suffix structure for mouse wheel events.
struct kan_platform_application_event_mouse_wheel_t
{
    kan_platform_window_id_t window_id;
    float wheel_x;
    float wheel_y;
    float window_x;
    float window_y;
};

/// \brief Describes event received from platform.
struct kan_platform_application_event_t
{
    enum kan_platform_application_event_type_t type;
    kan_time_size_t time_ns;

    union
    {
        struct kan_platform_application_event_display_t display;
        struct kan_platform_application_event_window_t window;
        struct kan_platform_application_event_keyboard_t keyboard;
        struct kan_platform_application_event_text_editing_t text_editing;
        struct kan_platform_application_event_text_input_t text_input;
        struct kan_platform_application_event_mouse_motion_t mouse_motion;
        struct kan_platform_application_event_mouse_button_t mouse_button;
        struct kan_platform_application_event_mouse_wheel_t mouse_wheel;
    };
};

/// \brief Enumerates basic platform themes.
enum kan_platform_system_theme_t
{
    KAN_PLATFORM_SYSTEM_THEME_LIGHT = 0,
    KAN_PLATFORM_SYSTEM_THEME_DARK,
    KAN_PLATFORM_SYSTEM_THEME_UNKNOWN,
};

/// \brief Basic structure for sharing integer bounds of visual objects on screen.
struct kan_platform_integer_bounds_t
{
    kan_platform_visual_offset_t min_x;
    kan_platform_visual_offset_t min_y;
    kan_platform_visual_offset_t max_x;
    kan_platform_visual_offset_t max_y;
};

/// \brief Enumerates display orientations.
enum kan_platform_display_orientation_t
{
    KAN_PLATFORM_DISPLAY_ORIENTATION_UNKNOWN = 0,
    KAN_PLATFORM_DISPLAY_ORIENTATION_LANDSCAPE,
    KAN_PLATFORM_DISPLAY_ORIENTATION_LANDSCAPE_FLIPPED,
    KAN_PLATFORM_DISPLAY_ORIENTATION_PORTRAIT,
    KAN_PLATFORM_DISPLAY_ORIENTATION_PORTRAIT_FLIPPED
};

/// \brief Describes display mode.
struct kan_platform_display_mode_t
{
    enum kan_platform_pixel_format_t pixel_format;
    kan_platform_visual_size_t width;
    kan_platform_visual_size_t height;
    float pixel_density;
    float refresh_rate;
};

/// \brief Enumerates supported window flags.
enum kan_platform_window_flag_t
{
    KAN_PLATFORM_WINDOW_FLAG_FULLSCREEN = 1u << 0u,
    KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_OPEN_GL = 1u << 1u,
    KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_VULKAN = 1u << 2u,
    KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_METAL = 1u << 3u,
    KAN_PLATFORM_WINDOW_FLAG_OCCLUDED = 1u << 4u,
    KAN_PLATFORM_WINDOW_FLAG_HIDDEN = 1u << 5u,
    KAN_PLATFORM_WINDOW_FLAG_BORDERLESS = 1u << 6u,
    KAN_PLATFORM_WINDOW_FLAG_TRANSPARENT = 1u << 7u,
    KAN_PLATFORM_WINDOW_FLAG_RESIZABLE = 1u << 8u,
    KAN_PLATFORM_WINDOW_FLAG_MINIMIZED = 1u << 9u,
    KAN_PLATFORM_WINDOW_FLAG_MAXIMIZED = 1u << 10u,
    KAN_PLATFORM_WINDOW_FLAG_UTILITY = 1u << 11u,
    KAN_PLATFORM_WINDOW_FLAG_TOOLTIP = 1u << 12u,
    KAN_PLATFORM_WINDOW_FLAG_POPUP = 1u << 13u,
    KAN_PLATFORM_WINDOW_FLAG_ALWAYS_ON_TOP = 1u << 14u,
    KAN_PLATFORM_WINDOW_FLAG_HIGH_PIXEL_DENSITY = 1u << 19u,
    KAN_PLATFORM_WINDOW_FLAG_MOUSE_GRABBED = 1u << 15u,
    KAN_PLATFORM_WINDOW_FLAG_KEYBOARD_GRABBED = 1u << 16u,
    KAN_PLATFORM_WINDOW_FLAG_INPUT_FOCUS = 1u << 17u,
    KAN_PLATFORM_WINDOW_FLAG_MOUSE_FOCUS = 1u << 18u,
    KAN_PLATFORM_WINDOW_FLAG_MOUSE_CAPTURE = 1u << 20u,
};

PLATFORM_API void kan_platform_application_event_init (struct kan_platform_application_event_t *instance);

PLATFORM_API void kan_platform_application_event_move (struct kan_platform_application_event_t *from,
                                                       struct kan_platform_application_event_t *to);

PLATFORM_API void kan_platform_application_event_shutdown (struct kan_platform_application_event_t *instance);

/// \brief Initializes application backend. Must be called on startup.
PLATFORM_API kan_bool_t kan_platform_application_init (void);

/// \brief Shutdown application backend. Must be called before exiting program.
PLATFORM_API void kan_platform_application_shutdown (void);

/// \brief Pops next platform event from platform events queue.
PLATFORM_API kan_bool_t kan_platform_application_fetch_next_event (struct kan_platform_application_event_t *output);

/// \brief Queries current system theme.
PLATFORM_API enum kan_platform_system_theme_t kan_platform_application_get_system_theme (void);

/// \brief Queries all connected display ids and outputs them to given array.
PLATFORM_API void kan_platform_application_get_display_ids (struct kan_dynamic_array_t *output_ids_array);

/// \brief Queries primary display id.
PLATFORM_API kan_platform_display_id_t kan_platform_application_get_primary_display_id (void);

/// \brief Queries display bounds.
PLATFORM_API kan_bool_t kan_platform_application_get_display_bounds (
    kan_platform_display_id_t display_id, struct kan_platform_integer_bounds_t *output_bounds);

/// \brief Queries display orientation.
PLATFORM_API enum kan_platform_display_orientation_t kan_platform_application_get_display_orientation (
    kan_platform_display_id_t display_id);

/// \brief Queries display content scale.
PLATFORM_API float kan_platform_application_get_display_content_scale (kan_platform_display_id_t display_id);

/// \brief Queries display modes for full screen rendering and outputs them into given array.
PLATFORM_API void kan_platform_application_get_fullscreen_display_modes (kan_platform_display_id_t display_id,
                                                                         struct kan_dynamic_array_t *output_array);

/// \brief Queries current used display mode.
PLATFORM_API kan_bool_t kan_platform_application_get_current_display_mode (kan_platform_display_id_t display_id,
                                                                           struct kan_platform_display_mode_t *output);

/// \brief Queries display mode used by user desktop. Might be different from current display mode.
PLATFORM_API kan_bool_t kan_platform_application_get_desktop_display_mode (kan_platform_display_id_t display_id,
                                                                           struct kan_platform_display_mode_t *output);

/// \brief Creates new window with given title, size and flags.
PLATFORM_API kan_platform_window_id_t kan_platform_application_window_create (const char *title,
                                                                              kan_platform_visual_size_t width,
                                                                              kan_platform_visual_size_t height,
                                                                              enum kan_platform_window_flag_t flags);

/// \brief Returns display associated with the given window.
PLATFORM_API kan_platform_display_id_t
kan_platform_application_window_get_display_id (kan_platform_window_id_t window_id);

/// \brief Queries pixel density for given window.
PLATFORM_API float kan_platform_application_window_get_pixel_density (kan_platform_window_id_t window_id);

/// \brief Queries display scale for given window.
PLATFORM_API float kan_platform_application_window_get_display_scale (kan_platform_window_id_t window_id);

/// \brief Queries pixel format for given window.
PLATFORM_API enum kan_platform_pixel_format_t kan_platform_application_window_get_pixel_format (
    kan_platform_window_id_t window_id);

/// \brief Switches window display to given mode and makes window fullscreen.
PLATFORM_API kan_bool_t kan_platform_application_window_enter_fullscreen (
    kan_platform_window_id_t window_id, const struct kan_platform_display_mode_t *display_mode);

/// \brief Switches window mode into non-fullscreen mode.
PLATFORM_API kan_bool_t kan_platform_application_window_leave_fullscreen (kan_platform_window_id_t window_id);

/// \brief Queries window status flags.
PLATFORM_API enum kan_platform_window_flag_t kan_platform_application_window_get_flags (
    kan_platform_window_id_t window_id);

/// \brief Updates window title.
PLATFORM_API kan_bool_t kan_platform_application_window_set_title (kan_platform_window_id_t window_id,
                                                                   const char *title);

/// \brief Queries window title.
PLATFORM_API const char *kan_platform_application_window_get_title (kan_platform_window_id_t window_id);

/// \brief Updates window icon.
/// \warning Pixel data is not copied and should be preserved by used.
PLATFORM_API kan_bool_t kan_platform_application_window_set_icon (kan_platform_window_id_t window_id,
                                                                  enum kan_platform_pixel_format_t pixel_format,
                                                                  kan_platform_visual_size_t width,
                                                                  kan_platform_visual_size_t height,
                                                                  const void *data);

/// \brief Updates window position.
PLATFORM_API kan_bool_t kan_platform_application_window_set_position (kan_platform_window_id_t window_id,
                                                                      kan_platform_visual_offset_t x,
                                                                      kan_platform_visual_offset_t y);

/// \brief Queries window position.
PLATFORM_API kan_bool_t kan_platform_application_window_get_position (kan_platform_window_id_t window_id,
                                                                      kan_platform_visual_offset_t *output_x,
                                                                      kan_platform_visual_offset_t *output_y);

/// \brief Updates window size.
PLATFORM_API kan_bool_t kan_platform_application_window_set_size (kan_platform_window_id_t window_id,
                                                                  kan_platform_visual_size_t width,
                                                                  kan_platform_visual_size_t height);

/// \brief Queries window size.
PLATFORM_API kan_bool_t kan_platform_application_window_get_size (kan_platform_window_id_t window_id,
                                                                  kan_platform_visual_size_t *output_width,
                                                                  kan_platform_visual_size_t *output_height);

/// \brief Queries window size for rendering: result size is always in pixels and not in window coordinates.
PLATFORM_API kan_bool_t kan_platform_application_window_get_size_for_render (kan_platform_window_id_t window_id,
                                                                             kan_platform_visual_size_t *output_width,
                                                                             kan_platform_visual_size_t *output_height);

/// \brief Updates window minimum size.
PLATFORM_API kan_bool_t kan_platform_application_window_set_minimum_size (kan_platform_window_id_t window_id,
                                                                          kan_platform_visual_size_t width,
                                                                          kan_platform_visual_size_t height);

/// \brief Queries window minimum size.
PLATFORM_API kan_bool_t kan_platform_application_window_get_minimum_size (kan_platform_window_id_t window_id,
                                                                          kan_platform_visual_size_t *output_width,
                                                                          kan_platform_visual_size_t *output_height);

/// \brief Updates window maximum size.
PLATFORM_API kan_bool_t kan_platform_application_window_set_maximum_size (kan_platform_window_id_t window_id,
                                                                          kan_platform_visual_size_t width,
                                                                          kan_platform_visual_size_t height);

/// \brief Queries window maximum size.
PLATFORM_API kan_bool_t kan_platform_application_window_get_maximum_size (kan_platform_window_id_t window_id,
                                                                          kan_platform_visual_size_t *output_width,
                                                                          kan_platform_visual_size_t *output_height);

/// \brief Sets whether window has borders.
PLATFORM_API kan_bool_t kan_platform_application_window_set_bordered (kan_platform_window_id_t window_id,
                                                                      kan_bool_t bordered);

/// \brief Sets whether window is resizeable.
PLATFORM_API kan_bool_t kan_platform_application_window_set_resizable (kan_platform_window_id_t window_id,
                                                                       kan_bool_t resizable);

/// \brief Sets whether window is always on top.
PLATFORM_API kan_bool_t kan_platform_application_window_set_always_on_top (kan_platform_window_id_t window_id,
                                                                           kan_bool_t always_on_top);

/// \brief Shows window to user.
PLATFORM_API kan_bool_t kan_platform_application_window_show (kan_platform_window_id_t window_id);

/// \brief Hides window from user.
PLATFORM_API kan_bool_t kan_platform_application_window_hide (kan_platform_window_id_t window_id);

/// \brief Raises window on user desktop and captures focus.
PLATFORM_API kan_bool_t kan_platform_application_window_raise (kan_platform_window_id_t window_id);

/// \brief Minimizes given window.
PLATFORM_API kan_bool_t kan_platform_application_window_minimize (kan_platform_window_id_t window_id);

/// \brief Maximizes given window.
PLATFORM_API kan_bool_t kan_platform_application_window_maximize (kan_platform_window_id_t window_id);

/// \brief Restores given window original size (before maximizing/minimizing).
PLATFORM_API kan_bool_t kan_platform_application_window_restore (kan_platform_window_id_t window_id);

/// \brief Sets whether given window grabs mouse.
PLATFORM_API kan_bool_t kan_platform_application_window_set_mouse_grab (kan_platform_window_id_t window_id,
                                                                        kan_bool_t grab_mouse);

/// \brief Sets whether given window grabs keyboard.
PLATFORM_API kan_bool_t kan_platform_application_window_set_keyboard_grab (kan_platform_window_id_t window_id,
                                                                           kan_bool_t grab_keyboard);

/// \brief Sets given window opacity.
PLATFORM_API kan_bool_t kan_platform_application_window_set_opacity (kan_platform_window_id_t window_id, float opacity);

/// \brief Queries given window opacity.
PLATFORM_API float kan_platform_application_window_get_opacity (kan_platform_window_id_t window_id);

/// \brief Sets whether given window is focusable.
PLATFORM_API void kan_platform_application_window_set_focusable (kan_platform_window_id_t window_id,
                                                                 kan_bool_t focusable);

/// \brief Attempts to create Vulkan surface on given window using given Vulkan instance.
PLATFORM_API uint64_t kan_platform_application_window_create_vulkan_surface (kan_platform_window_id_t window_id,
                                                                             kan_memory_size_t vulkan_instance,
                                                                             void *vulkan_allocation_callbacks);

/// \brief Destroys Vulkan surface that was created for given window using given Vulkan instance.
PLATFORM_API void kan_platform_application_window_destroy_vulkan_surface (kan_platform_window_id_t window_id,
                                                                          kan_memory_size_t vulkan_instance,
                                                                          uint64_t vulkan_surface,
                                                                          void *vulkan_allocation_callbacks);

/// \brief Destroys given window.
PLATFORM_API void kan_platform_application_window_destroy (kan_platform_window_id_t window_id);

/// \brief Queries mouse state and position local to focus window.
PLATFORM_API uint8_t kan_platform_application_get_mouse_state_local_to_focus (float *x, float *y);

/// \brief Queries mouse state and position in global coordinates.
PLATFORM_API uint8_t kan_platform_application_get_global_mouse_state (float *x, float *y);

/// \brief Warps mouse to given position in given window.
PLATFORM_API void kan_platform_application_warp_mouse_in_window (kan_platform_window_id_t window_id, float x, float y);

/// \brief Warps mouse to given global position.
PLATFORM_API void kan_platform_application_warp_mouse_global (float x, float y);

/// \brief Sets whether cursor is visible.
PLATFORM_API void kan_platform_application_set_cursor_visible (kan_bool_t visible);

/// \brief Returns allocation group used for clipboard-related allocations.
PLATFORM_API kan_allocation_group_t kan_platform_application_get_clipboard_allocation_group (void);

/// \brief Extracts text from given clipboard using `kan_platform_application_get_clipboard_allocation_group`
///        for allocation. Returned text should be deallocated when no longer used.
PLATFORM_API char *kan_platform_application_extract_text_from_clipboard (void);

/// \brief Puts given text into platform clipboard.
PLATFORM_API void kan_platform_application_put_text_into_clipboard (const char *text);

/// \brief Adds request for the Vulkan library to be loaded. Returns true if library is loaded and ready to be used.
PLATFORM_API kan_bool_t kan_platform_application_register_vulkan_library_usage (void);

/// \brief Attempts to find and return address for `vkGetInstanceProcAddr` function.
/// \invariant Vulkan library should be in loaded state.
PLATFORM_API void *kan_platform_application_request_vulkan_resolve_function (void);

/// \brief Attempts to request count of supported instance extensions for Vulkan library.
/// \details All `char *` strings added to output array will be allocated in given allocation group.
/// \invariant Output array must not be initialized.
/// \invariant Vulkan library should be in loaded state.
PLATFORM_API void kan_platform_application_request_vulkan_extensions (struct kan_dynamic_array_t *output,
                                                                      kan_allocation_group_t allocation_group);

/// \brief Removes request for the Vulkan library loading. Call it when Vulkan is no longer required.
PLATFORM_API void kan_platform_application_unregister_vulkan_library_usage (void);

KAN_C_HEADER_END
