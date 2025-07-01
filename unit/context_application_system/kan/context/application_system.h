#pragma once

#include <context_application_system_api.h>

#include <kan/api_common/c_header.h>
#include <kan/context/context.h>
#include <kan/platform/application.h>

/// \file
/// \brief Contains API for context application system -- system for accessing application data from non-main threads.
///
/// \par Definition
/// \parblock
/// Due to platform specific requirements, platform unit features should only be accessed from main thread. It makes
/// managing objects like windows inconvenient and difficult. Application system solves this issue by proving interface
/// for non-main thread access to platform application features, using operation queue and information buffering.
/// \endparblock
///
/// \par Usage
/// \parblock
/// In order for system to be usable, it needs two special functions to be called on main thread properly:
/// - `kan_application_system_sync_in_main_thread` needs to be called every frame to synchronize information and
///   execute delayed operations on main thread.
/// - `kan_application_system_prepare_for_destroy_in_main_thread` needs to be called on main thread before context
///   destruction. It allows application system to destroy everything in the correct thread.
///
/// All other functions can be called from any thread and are fully thread safe. The only requirement is that user
/// must guarantee that no functions are called during execution of main thread functions.
/// \endparblock
///
/// \par Window handles
/// \parblock
/// Application system introduces window handles system. In lots of cases we need to create window and then start
/// working with it right away, but in case of application system, creation is delayed, therefore we don't have window
/// id after queuing creation operation. To solve this issue, window handles were introduced: they replace window ids
/// for application system and make it possible to refer to any window before it is created. Window handles are
/// invalidated on window destruction.
/// \endparblock
///
/// \par Window resources
/// \parblock
/// In some cases, usually for render implementation, it is required to create and manage custom resources that are
/// strictly attached to window lifecycle and therefore should be managed from the same thread as window without async
/// calls. Common example is surfaces and swap chains for render backends.
///
/// In order to support this, concept of window resources was introduced. Any resource can be bound to window using
/// `kan_application_system_window_resource_binding_t` with `init` and `shutdown` callbacks. `init` will be called on
/// the main thread as soon as window is ready and able to work with resources. `shutdown` will be called before window
/// destruction or inside command execution routine if resource was manually detached without window destruction.
/// Therefore, through these 2 callbacks it is possible to attach and manage arbitrary window resources.
/// \endparblock

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_application_system_event_iterator_t);

/// \brief Contains buffered information about available display.
struct kan_application_system_display_info_t
{
    kan_platform_display_id_t id;
    struct kan_platform_integer_bounds_t bounds;
    enum kan_platform_display_orientation_t orientation;
    float content_scale;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_platform_display_mode_t)
    struct kan_dynamic_array_t fullscreen_modes;

    struct kan_platform_display_mode_t current_mode;
    struct kan_platform_display_mode_t desktop_mode;
};

KAN_HANDLE_DEFINE (kan_application_system_display_info_iterator_t);

KAN_HANDLE_DEFINE (kan_application_system_window_t);

/// \brief Contains buffered information about available window.
struct kan_application_system_window_info_t
{
    kan_application_system_window_t handle;
    kan_platform_window_id_t id;
    kan_platform_display_id_t display_id;

    float pixel_density;
    float display_scale;
    float opacity;

    enum kan_platform_pixel_format_t pixel_format;
    enum kan_platform_window_flag_t flags;
    struct kan_platform_integer_bounds_t bounds;

    /// \details Always in pixels, not in window coordinates, therefore more suitable for rendering.
    kan_platform_visual_size_t width_for_render;

    /// \details Always in pixels, not in window coordinates, therefore more suitable for rendering.
    kan_platform_visual_size_t height_for_render;

    kan_platform_visual_size_t minimum_width;
    kan_platform_visual_size_t minimum_height;

    kan_platform_visual_size_t maximum_width;
    kan_platform_visual_size_t maximum_height;
};

KAN_HANDLE_DEFINE (kan_application_system_window_info_iterator_t);

/// \brief Describes window resource binding with its user data and callbacks.
struct kan_application_system_window_resource_binding_t
{
    void *user_data;
    void (*init) (void *user_data, const struct kan_application_system_window_info_t *window_info);
    void (*shutdown) (void *user_data, const struct kan_application_system_window_info_t *window_info);
};

/// \brief Resource binding id that can be used to manually remove resource without destroying window.
KAN_TYPED_ID_32_DEFINE (kan_application_system_window_resource_id_t);

/// \brief Contains buffered information about mouse state.
struct kan_application_system_mouse_state_t
{
    uint8_t button_mask;
    float local_x;
    float local_y;
    float global_x;
    float global_y;
};

/// \brief Synchronizes application state with application system.
/// \invariant Should be called in main thread every frame.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_sync_in_main_thread (kan_context_system_t system_handle);

/// \brief Prepares application system to be destroyed.
/// \invariant Should be called in main thread before context destruction.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_prepare_for_destroy_in_main_thread (
    kan_context_system_t system_handle);

/// \brief Creates new iterator for application events.
CONTEXT_APPLICATION_SYSTEM_API kan_application_system_event_iterator_t
kan_application_system_event_iterator_create (kan_context_system_t system_handle);

/// \brief Returns new application event or NULL if there is no more events.
CONTEXT_APPLICATION_SYSTEM_API const struct kan_platform_application_event_t *
kan_application_system_event_iterator_get (kan_context_system_t system_handle,
                                           kan_application_system_event_iterator_t event_iterator);

/// \brief Advances given iterator to the next event and returns new iterator value.
CONTEXT_APPLICATION_SYSTEM_API kan_application_system_event_iterator_t
kan_application_system_event_iterator_advance (kan_application_system_event_iterator_t event_iterator);

/// \brief Destroys given application event iterator.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_event_iterator_destroy (
    kan_context_system_t system_handle, kan_application_system_event_iterator_t event_iterator);

/// \brief Creates new iterator for querying information about displays.
/// \invariant These iterators and display info overall are invalidated during sync, do not store them!
CONTEXT_APPLICATION_SYSTEM_API kan_application_system_display_info_iterator_t
kan_application_system_display_info_iterator_create (kan_context_system_t system_handle);

/// \brief Returns next display info or NULL if there is no more display info.
CONTEXT_APPLICATION_SYSTEM_API const struct kan_application_system_display_info_t *
kan_application_system_display_info_iterator_get (kan_application_system_display_info_iterator_t display_info_iterator);

/// \brief Advances display info iterator to the next display info and returns new iterator value.
CONTEXT_APPLICATION_SYSTEM_API kan_application_system_display_info_iterator_t
kan_application_system_display_info_iterator_advance (
    kan_application_system_display_info_iterator_t display_info_iterator);

/// \brief Creates new iterator for querying information about windows.
/// \invariant These iterators are invalidated during sync, do not store them!
CONTEXT_APPLICATION_SYSTEM_API kan_application_system_window_info_iterator_t
kan_application_system_window_info_iterator_create (kan_context_system_t system_handle);

/// \brief Returns next window info or NULL if there is no more window info.
CONTEXT_APPLICATION_SYSTEM_API const struct kan_application_system_window_info_t *
kan_application_system_window_info_iterator_get (kan_application_system_window_info_iterator_t window_info_iterator);

/// \brief Advances display info iterator to the next window info and returns new iterator value.
CONTEXT_APPLICATION_SYSTEM_API kan_application_system_window_info_iterator_t
kan_application_system_window_info_iterator_advance (
    kan_application_system_window_info_iterator_t window_info_iterator);

/// \brief Queries window info by window handle.
CONTEXT_APPLICATION_SYSTEM_API const struct kan_application_system_window_info_t *
kan_application_system_get_window_info_from_handle (kan_context_system_t system_handle,
                                                    kan_application_system_window_t window_handle);

/// \brief Adapts `kan_platform_application_window_create`.
CONTEXT_APPLICATION_SYSTEM_API kan_application_system_window_t
kan_application_system_window_create (kan_context_system_t system_handle,
                                      const char *title,
                                      kan_platform_visual_size_t width,
                                      kan_platform_visual_size_t height,
                                      enum kan_platform_window_flag_t flags);

/// \brief Adapts `kan_platform_application_window_enter_fullscreen`.
/// \invariant `display_mode` should point to one of the display modes from display infos.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_enter_fullscreen (
    kan_context_system_t system_handle,
    kan_application_system_window_t window_handle,
    const struct kan_platform_display_mode_t *display_mode);

/// \brief Adapts `kan_platform_application_window_leave_fullscreen`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_leave_fullscreen (
    kan_context_system_t system_handle, kan_application_system_window_t window_handle);

/// \brief Adapts `kan_platform_application_window_set_title`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_set_title (
    kan_context_system_t system_handle, kan_application_system_window_t window_handle, const char *title);

/// \brief Adapts `kan_platform_application_window_set_icon`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_set_icon (
    kan_context_system_t system_handle,
    kan_application_system_window_t window_handle,
    enum kan_platform_pixel_format_t pixel_format,
    kan_platform_visual_size_t width,
    kan_platform_visual_size_t height,
    const void *data);

/// \brief Sets window bounds, combines `kan_platform_application_window_set_position` and
///        `kan_platform_application_window_set_size`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_set_bounds (
    kan_context_system_t system_handle,
    kan_application_system_window_t window_handle,
    struct kan_platform_integer_bounds_t bounds);

/// \brief Adapts `kan_platform_application_window_set_minimum_size`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_set_minimum_size (
    kan_context_system_t system_handle,
    kan_application_system_window_t window_handle,
    kan_platform_visual_size_t minimum_width,
    kan_platform_visual_size_t minimum_height);

/// \brief Adapts `kan_platform_application_window_set_maximum_size`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_set_maximum_size (
    kan_context_system_t system_handle,
    kan_application_system_window_t window_handle,
    kan_platform_visual_size_t maximum_width,
    kan_platform_visual_size_t maximum_height);

/// \brief Adapts `kan_platform_application_window_set_bordered`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_set_bordered (
    kan_context_system_t system_handle, kan_application_system_window_t window_handle, bool bordered);

/// \brief Adapts `kan_platform_application_window_set_resizable`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_set_resizable (
    kan_context_system_t system_handle, kan_application_system_window_t window_handle, bool resizable);

/// \brief Adapts `kan_platform_application_window_set_always_on_top`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_set_always_on_top (
    kan_context_system_t system_handle, kan_application_system_window_t window_handle, bool always_on_top);

/// \brief Adapts `kan_platform_application_window_show`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_show (kan_context_system_t system_handle,
                                                                        kan_application_system_window_t window_handle);

/// \brief Adapts `kan_platform_application_window_hide`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_hide (kan_context_system_t system_handle,
                                                                        kan_application_system_window_t window_handle);

/// \brief Adapts `kan_platform_application_window_raise`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_raise (kan_context_system_t system_handle,
                                                                         kan_application_system_window_t window_handle);

/// \brief Adapts `kan_platform_application_window_minimize`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_minimize (
    kan_context_system_t system_handle, kan_application_system_window_t window_handle);

/// \brief Adapts `kan_platform_application_window_maximize`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_maximize (
    kan_context_system_t system_handle, kan_application_system_window_t window_handle);

/// \brief Adapts `kan_platform_application_window_restore`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_restore (
    kan_context_system_t system_handle, kan_application_system_window_t window_handle);

/// \brief Adapts `kan_platform_application_window_set_mouse_grab`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_set_mouse_grab (
    kan_context_system_t system_handle, kan_application_system_window_t window_handle, bool mouse_grab);

/// \brief Adapts `kan_platform_application_window_set_keyboard_grab`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_set_keyboard_grab (
    kan_context_system_t system_handle, kan_application_system_window_t window_handle, bool keyboard_grab);

/// \brief Adapts `kan_platform_application_window_set_opacity`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_set_opacity (
    kan_context_system_t system_handle, kan_application_system_window_t window_handle, float opacity);

/// \brief Adapts `kan_platform_application_window_set_focusable`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_window_set_focusable (kan_context_system_t system_handle,
                                                                          kan_application_system_window_t window_handle,
                                                                          bool focusable);

/// \brief Informs system that there is a logical component that expects text input in given window.
/// \details If it is the only listener, causes on screen keyboard to show up on systems that need it.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_window_add_text_listener (
    kan_context_system_t system_handle, kan_application_system_window_t window_handle);

/// \brief Informs system that logical component that expected text input in given window is no longer expecting it.
/// \details If it is the only listener, causes on screen keyboard to hide on systems that use it.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_window_remove_text_listener (
    kan_context_system_t system_handle, kan_application_system_window_t window_handle);

/// \brief Attaches new resource to given window and returns this resource id.
CONTEXT_APPLICATION_SYSTEM_API kan_application_system_window_resource_id_t
kan_application_system_window_add_resource (kan_context_system_t system_handle,
                                            kan_application_system_window_t window_handle,
                                            struct kan_application_system_window_resource_binding_t binding);

/// \brief Removes resource from window using this resource id without destroying the window.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_remove_resource (
    kan_context_system_t system_handle,
    kan_application_system_window_t window_handle,
    kan_application_system_window_resource_id_t resource_id);

/// \brief Adapts `kan_platform_application_window_destroy`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_destroy (
    kan_context_system_t system_handle, kan_application_system_window_t window_handle);

/// \brief Returns buffered mouse state.
CONTEXT_APPLICATION_SYSTEM_API const struct kan_application_system_mouse_state_t *
kan_application_system_get_mouse_state (kan_context_system_t system_handle);

/// \brief Adapts `kan_platform_application_warp_mouse_global`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_warp_mouse_global (kan_context_system_t system_handle,
                                                                              float global_x,
                                                                              float global_y);

/// \brief Adapts `kan_platform_application_warp_mouse_in_window`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_warp_mouse_to_window (
    kan_context_system_t system_handle, kan_application_system_window_t window_handle, float local_x, float local_y);

/// \brief Adapts `kan_platform_application_system_set_cursor_visible`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_set_cursor_visible (kan_context_system_t system_handle,
                                                                               bool cursor_visible);

/// \brief Returns buffered clipboard text.
CONTEXT_APPLICATION_SYSTEM_API const char *kan_application_system_clipboard_get_text (
    kan_context_system_t system_handle);

/// \brief Adapts `kan_platform_application_put_text_into_clipboard`.
CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_clipboard_set_text (kan_context_system_t system_handle,
                                                                               const char *text);

KAN_C_HEADER_END
