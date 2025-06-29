#include <stddef.h>
#include <string.h>

#include <kan/container/event_queue.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/context/application_system.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/error/critical.h>
#include <kan/memory/allocation.h>
#include <kan/threading/atomic.h>

struct event_node_t
{
    struct kan_event_queue_node_t node;
    struct kan_platform_application_event_t event;
};

struct display_info_holder_t
{
    struct display_info_holder_t *next;
    struct kan_application_system_display_info_t info;
};

struct window_resource_t
{
    struct window_resource_t *previous;
    struct window_resource_t *next;
    kan_application_system_window_resource_id_t id;
    struct kan_application_system_window_resource_binding_t binding;
};

struct window_info_holder_t
{
    struct window_info_holder_t *previous;
    struct window_info_holder_t *next;
    struct kan_application_system_window_info_t info;
    kan_instance_size_t text_input_listeners;
    struct window_resource_t *first_resource;
};

enum operation_type_t
{
    OPERATION_TYPE_WINDOW_CREATE = 0u,
    OPERATION_TYPE_WINDOW_ENTER_FULLSCREEN,
    OPERATION_TYPE_WINDOW_LEAVE_FULLSCREEN,
    OPERATION_TYPE_WINDOW_SET_TITLE,
    OPERATION_TYPE_WINDOW_SET_ICON,
    OPERATION_TYPE_WINDOW_SET_BOUNDS,
    OPERATION_TYPE_WINDOW_SET_MINIMUM_SIZE,
    OPERATION_TYPE_WINDOW_SET_MAXIMUM_SIZE,
    OPERATION_TYPE_WINDOW_SET_BORDERED,
    OPERATION_TYPE_WINDOW_SET_RESIZEABLE,
    OPERATION_TYPE_WINDOW_SET_ALWAYS_ON_TOP,
    OPERATION_TYPE_WINDOW_SHOW,
    OPERATION_TYPE_WINDOW_HIDE,
    OPERATION_TYPE_WINDOW_RAISE,
    OPERATION_TYPE_WINDOW_MINIMIZE,
    OPERATION_TYPE_WINDOW_MAXIMIZE,
    OPERATION_TYPE_WINDOW_RESTORE,
    OPERATION_TYPE_WINDOW_SET_MOUSE_GRAB,
    OPERATION_TYPE_WINDOW_SET_KEYBOARD_GRAB,
    OPERATION_TYPE_WINDOW_SET_OPACITY,
    OPERATION_TYPE_WINDOW_SET_FOCUSABLE,
    OPERATION_TYPE_WINDOW_ADD_RESOURCE,
    OPERATION_TYPE_WINDOW_REMOVE_RESOURCE,
    OPERATION_TYPE_WINDOW_ADD_TEXT_LISTENER,
    OPERATION_TYPE_WINDOW_REMOVE_TEXT_LISTENER,
    OPERATION_TYPE_WINDOW_DESTROY,
    OPERATION_TYPE_WARP_MOUSE_GLOBAL,
    OPERATION_TYPE_WARP_MOUSE_TO_WINDOW,
    OPERATION_TYPE_SET_CURSOR_VISIBLE,
    OPERATION_TYPE_CLIPBOARD_SET_TEXT,
};

struct window_create_suffix_t
{
    kan_application_system_window_t window_handle;
    const char *title;
    kan_platform_visual_size_t width;
    kan_platform_visual_size_t height;
    enum kan_platform_window_flag_t flags;
};

struct window_enter_fullscreen_suffix_t
{
    kan_application_system_window_t window_handle;
    const struct kan_platform_display_mode_t *display_mode;
};

struct window_parameterless_suffix_t
{
    kan_application_system_window_t window_handle;
};

struct window_set_textual_parameter_suffix_t
{
    kan_application_system_window_t window_handle;
    char *data;
};

struct window_set_icon_suffix_t
{
    kan_application_system_window_t window_handle;
    enum kan_platform_pixel_format_t pixel_format;
    kan_platform_visual_size_t width;
    kan_platform_visual_size_t height;
    const void *data;
};

struct window_set_bounds_suffix_t
{
    kan_application_system_window_t window_handle;
    struct kan_platform_integer_bounds_t bounds;
};

struct window_set_size_limit_suffix_t
{
    kan_application_system_window_t window_handle;
    kan_platform_visual_size_t width;
    kan_platform_visual_size_t height;
};

struct window_set_boolean_parameter_suffix_t
{
    kan_application_system_window_t window_handle;
    bool value;
};

struct window_set_floating_point_parameter_suffix_t
{
    kan_application_system_window_t window_handle;
    float value;
};

struct window_add_resource_suffix_t
{
    kan_application_system_window_t window_handle;
    kan_application_system_window_resource_id_t resource_id;
    struct kan_application_system_window_resource_binding_t resource_binding;
};

struct window_remove_resource_suffix_t
{
    kan_application_system_window_t window_handle;
    kan_application_system_window_resource_id_t resource_id;
};

struct warp_mouse_global_suffix_t
{
    float global_x;
    float global_y;
};

struct warp_mouse_to_window_suffix_t
{
    kan_application_system_window_t window_handle;
    float local_x;
    float local_y;
};

struct set_cursor_visible_suffix_t
{
    bool visible;
};

struct clipboard_set_text_suffix_t
{
    char *text;
};

struct operation_t
{
    struct operation_t *next;
    enum operation_type_t type;

    union
    {
        struct window_create_suffix_t window_create_suffix;
        struct window_enter_fullscreen_suffix_t window_enter_fullscreen_suffix;
        struct window_parameterless_suffix_t window_parameterless_suffix;
        struct window_set_textual_parameter_suffix_t window_set_textual_parameter_suffix;
        struct window_set_icon_suffix_t window_set_icon_suffix;
        struct window_set_bounds_suffix_t window_set_bounds_suffix;
        struct window_set_size_limit_suffix_t window_set_size_limit_suffix;
        struct window_set_boolean_parameter_suffix_t window_set_boolean_parameter_suffix;
        struct window_set_floating_point_parameter_suffix_t window_set_floating_point_parameter_suffix;
        struct window_add_resource_suffix_t window_add_resource_suffix;
        struct window_remove_resource_suffix_t window_remove_resource_suffix;
        struct warp_mouse_global_suffix_t warp_mouse_global_suffix;
        struct warp_mouse_to_window_suffix_t warp_mouse_to_window_suffix;
        struct set_cursor_visible_suffix_t set_cursor_visible_suffix;
        struct clipboard_set_text_suffix_t clipboard_set_text_suffix;
    };
};

struct application_system_t
{
    kan_context_t context;
    kan_allocation_group_t group;
    kan_allocation_group_t events_group;
    kan_allocation_group_t display_infos_group;
    kan_allocation_group_t window_infos_group;
    kan_allocation_group_t operations_group;
    kan_allocation_group_t clipboard_group;

    kan_cpu_section_t sync_main_section;
    kan_cpu_section_t sync_info_section;

    struct kan_atomic_int_t operation_submission_lock;
    struct kan_stack_group_allocator_t operation_temporary_allocator;
    struct kan_event_queue_t event_queue;

    struct display_info_holder_t *first_display_info;
    struct window_info_holder_t *first_window_info;
    struct operation_t *first_operation;
    struct operation_t *last_operation;

    struct kan_application_system_mouse_state_t mouse_state;
    char *clipboard_content;
    bool initial_clipboard_update_done;
    struct kan_atomic_int_t resource_id_counter;
};

static inline struct event_node_t *allocate_event_node (kan_allocation_group_t events_group)
{
    struct event_node_t *node = kan_allocate_batched (events_group, sizeof (struct event_node_t));
    kan_platform_application_event_init (&node->event);
    return node;
}

static inline void free_event_node (struct event_node_t *instance, kan_allocation_group_t events_group)
{
    kan_platform_application_event_shutdown (&instance->event);
    kan_free_batched (events_group, instance);
}

static inline void shutdown_operation (struct operation_t *operation, kan_allocation_group_t operation_allocation_group)
{
    if (operation->type == OPERATION_TYPE_CLIPBOARD_SET_TEXT && operation->clipboard_set_text_suffix.text)
    {
        kan_free_general (operation_allocation_group, operation->clipboard_set_text_suffix.text,
                          strlen (operation->clipboard_set_text_suffix.text) + 1u);
    }
}

kan_context_system_t application_system_create (kan_allocation_group_t group, void *user_config)
{
    struct application_system_t *system =
        kan_allocate_general (group, sizeof (struct application_system_t), alignof (struct application_system_t));

    system->group = group;
    system->events_group = kan_allocation_group_get_child (group, "events");
    system->display_infos_group = kan_allocation_group_get_child (group, "display_infos");
    system->window_infos_group = kan_allocation_group_get_child (group, "window_infos");
    system->operations_group = kan_allocation_group_get_child (group, "operations");
    system->clipboard_group = kan_allocation_group_get_child (group, "clipboard");

    system->sync_main_section = kan_cpu_section_get ("context_application_system_sync_in_main_thread");
    system->sync_info_section = kan_cpu_section_get ("sync_info");

    system->operation_submission_lock = kan_atomic_int_init (0);
    kan_stack_group_allocator_init (&system->operation_temporary_allocator, system->operations_group,
                                    KAN_APPLICATION_SYSTEM_OPERATION_STACK_SIZE);
    kan_event_queue_init (&system->event_queue, &allocate_event_node (system->events_group)->node);

    system->first_display_info = NULL;
    system->first_window_info = NULL;
    system->first_operation = NULL;
    system->last_operation = NULL;

    system->clipboard_content = NULL;
    system->initial_clipboard_update_done = false;
    system->resource_id_counter = kan_atomic_int_init (0);
    return KAN_HANDLE_SET (kan_context_system_t, system);
}

void application_system_connect (kan_context_system_t handle, kan_context_t context)
{
    struct application_system_t *system = KAN_HANDLE_GET (handle);
    system->context = context;
}

void application_system_init (kan_context_system_t handle) {}

void application_system_shutdown (kan_context_system_t handle) {}

void application_system_disconnect (kan_context_system_t handle) {}

static inline void application_system_clean_display_info_since (struct application_system_t *system,
                                                                struct display_info_holder_t *holder)
{
    while (holder)
    {
        struct display_info_holder_t *next = holder->next;
        kan_dynamic_array_shutdown (&holder->info.fullscreen_modes);
        kan_free_batched (system->display_infos_group, holder);
        holder = next;
    }
}

void application_system_destroy (kan_context_system_t handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (handle);
    application_system_clean_display_info_since (system, system->first_display_info);
    struct window_info_holder_t *holder = system->first_window_info;

    while (holder)
    {
        struct window_info_holder_t *next = holder->next;
        // Windows must be destroyed by kan_application_system_prepare_for_destroy_in_main_thread already.
        KAN_ASSERT (!KAN_TYPED_ID_32_IS_VALID (holder->info.id))
        kan_free_batched (system->window_infos_group, holder);
        holder = next;
    }

    struct operation_t *operation = system->first_operation;
    while (operation)
    {
        struct operation_t *next = operation->next;
        shutdown_operation (operation, system->operations_group);
        operation = next;
    }

    kan_stack_group_allocator_shutdown (&system->operation_temporary_allocator);
    struct event_node_t *event_node = (struct event_node_t *) system->event_queue.oldest;

    while (event_node)
    {
        struct event_node_t *next = (struct event_node_t *) event_node->node.next;
        free_event_node (event_node, system->events_group);
        event_node = next;
    }

    if (system->clipboard_content)
    {
        kan_free_general (system->clipboard_group, system->clipboard_content, strlen (system->clipboard_content) + 1u);
    }

    kan_free_general (system->group, system, sizeof (struct application_system_t));
}

CONTEXT_APPLICATION_SYSTEM_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (application_system_t) = {
    .name = "application_system_t",
    .create = application_system_create,
    .connect = application_system_connect,
    .connected_init = application_system_init,
    .connected_shutdown = application_system_shutdown,
    .disconnect = application_system_disconnect,
    .destroy = application_system_destroy,
};

static inline void destroy_window_resources (struct application_system_t *system, struct window_info_holder_t *holder)
{
    struct window_resource_t *resource = holder->first_resource;
    while (resource)
    {
        struct window_resource_t *next = resource->next;
        resource->binding.shutdown (resource->binding.user_data, &holder->info);
        kan_free_batched (system->window_infos_group, resource);
        resource = next;
    }

    holder->first_resource = NULL;
}

static inline void flush_operations (struct application_system_t *system)
{
    struct operation_t *operation = system->first_operation;
    while (operation)
    {
        switch (operation->type)
        {
        case OPERATION_TYPE_WINDOW_CREATE:
        {
            struct window_info_holder_t *holder = KAN_HANDLE_GET (operation->window_create_suffix.window_handle);
            KAN_ASSERT (!KAN_TYPED_ID_32_IS_VALID (holder->info.id))

            holder->info.id = kan_platform_application_window_create (
                operation->window_create_suffix.title, operation->window_create_suffix.width,
                operation->window_create_suffix.height, operation->window_create_suffix.flags);

            // Text input can be enabled by default on desktop platforms as it only affects events.
            // We'd like to have similar debuggable behavior on desktop and other platforms, so we disable it.
            kan_platform_application_window_set_text_input_enabled (holder->info.id, false);

            // Technically, there is no way for resources to be attached before window creation.
            KAN_ASSERT (!holder->first_resource)
            break;
        }

        case OPERATION_TYPE_WINDOW_ENTER_FULLSCREEN:
        {
            struct window_info_holder_t *holder =
                KAN_HANDLE_GET (operation->window_enter_fullscreen_suffix.window_handle);

            kan_platform_application_window_enter_fullscreen (holder->info.id,
                                                              operation->window_enter_fullscreen_suffix.display_mode);

            break;
        }

        case OPERATION_TYPE_WINDOW_LEAVE_FULLSCREEN:
        {
            struct window_info_holder_t *holder = KAN_HANDLE_GET (operation->window_parameterless_suffix.window_handle);

            kan_platform_application_window_leave_fullscreen (holder->info.id);
            break;
        }

        case OPERATION_TYPE_WINDOW_SET_TITLE:
        {
            struct window_info_holder_t *holder =
                KAN_HANDLE_GET (operation->window_set_textual_parameter_suffix.window_handle);

            kan_platform_application_window_set_title (holder->info.id,
                                                       operation->window_set_textual_parameter_suffix.data);
            break;
        }

        case OPERATION_TYPE_WINDOW_SET_ICON:
        {
            struct window_info_holder_t *holder = KAN_HANDLE_GET (operation->window_set_icon_suffix.window_handle);

            kan_platform_application_window_set_icon (holder->info.id, operation->window_set_icon_suffix.pixel_format,
                                                      operation->window_set_icon_suffix.width,
                                                      operation->window_set_icon_suffix.height,
                                                      operation->window_set_icon_suffix.data);
            break;
        }

        case OPERATION_TYPE_WINDOW_SET_BOUNDS:
        {
            struct window_info_holder_t *holder = KAN_HANDLE_GET (operation->window_set_bounds_suffix.window_handle);

            kan_platform_application_window_set_position (holder->info.id,
                                                          operation->window_set_bounds_suffix.bounds.min_x,
                                                          operation->window_set_bounds_suffix.bounds.min_y);

            kan_platform_application_window_set_size (
                holder->info.id,
                operation->window_set_bounds_suffix.bounds.max_x - operation->window_set_bounds_suffix.bounds.min_x,
                operation->window_set_bounds_suffix.bounds.max_y - operation->window_set_bounds_suffix.bounds.min_y);
            break;
        }

        case OPERATION_TYPE_WINDOW_SET_MINIMUM_SIZE:
        {
            struct window_info_holder_t *holder =
                KAN_HANDLE_GET (operation->window_set_size_limit_suffix.window_handle);

            kan_platform_application_window_set_minimum_size (holder->info.id,
                                                              operation->window_set_size_limit_suffix.width,
                                                              operation->window_set_size_limit_suffix.height);
            break;
        }

        case OPERATION_TYPE_WINDOW_SET_MAXIMUM_SIZE:
        {
            struct window_info_holder_t *holder =
                KAN_HANDLE_GET (operation->window_set_size_limit_suffix.window_handle);

            kan_platform_application_window_set_maximum_size (holder->info.id,
                                                              operation->window_set_size_limit_suffix.width,
                                                              operation->window_set_size_limit_suffix.height);
            break;
        }

        case OPERATION_TYPE_WINDOW_SET_BORDERED:
        {
            struct window_info_holder_t *holder =
                KAN_HANDLE_GET (operation->window_set_boolean_parameter_suffix.window_handle);

            kan_platform_application_window_set_bordered (holder->info.id,
                                                          operation->window_set_boolean_parameter_suffix.value);
            break;
        }

        case OPERATION_TYPE_WINDOW_SET_RESIZEABLE:
        {
            struct window_info_holder_t *holder =
                KAN_HANDLE_GET (operation->window_set_boolean_parameter_suffix.window_handle);

            kan_platform_application_window_set_resizable (holder->info.id,
                                                           operation->window_set_boolean_parameter_suffix.value);
            break;
        }

        case OPERATION_TYPE_WINDOW_SET_ALWAYS_ON_TOP:
        {
            struct window_info_holder_t *holder =
                KAN_HANDLE_GET (operation->window_set_boolean_parameter_suffix.window_handle);

            kan_platform_application_window_set_always_on_top (holder->info.id,
                                                               operation->window_set_boolean_parameter_suffix.value);
            break;
        }

        case OPERATION_TYPE_WINDOW_SHOW:
        {
            struct window_info_holder_t *holder = KAN_HANDLE_GET (operation->window_parameterless_suffix.window_handle);

            kan_platform_application_window_show (holder->info.id);
            break;
        }

        case OPERATION_TYPE_WINDOW_HIDE:
        {
            struct window_info_holder_t *holder = KAN_HANDLE_GET (operation->window_parameterless_suffix.window_handle);

            kan_platform_application_window_hide (holder->info.id);
            break;
        }

        case OPERATION_TYPE_WINDOW_RAISE:
        {
            struct window_info_holder_t *holder = KAN_HANDLE_GET (operation->window_parameterless_suffix.window_handle);

            kan_platform_application_window_raise (holder->info.id);
            break;
        }

        case OPERATION_TYPE_WINDOW_MINIMIZE:
        {
            struct window_info_holder_t *holder = KAN_HANDLE_GET (operation->window_parameterless_suffix.window_handle);

            kan_platform_application_window_minimize (holder->info.id);
            break;
        }

        case OPERATION_TYPE_WINDOW_MAXIMIZE:
        {
            struct window_info_holder_t *holder = KAN_HANDLE_GET (operation->window_parameterless_suffix.window_handle);

            kan_platform_application_window_maximize (holder->info.id);
            break;
        }

        case OPERATION_TYPE_WINDOW_RESTORE:
        {
            struct window_info_holder_t *holder = KAN_HANDLE_GET (operation->window_parameterless_suffix.window_handle);

            kan_platform_application_window_restore (holder->info.id);
            break;
        }

        case OPERATION_TYPE_WINDOW_SET_MOUSE_GRAB:
        {
            struct window_info_holder_t *holder =
                KAN_HANDLE_GET (operation->window_set_boolean_parameter_suffix.window_handle);

            kan_platform_application_window_set_mouse_grab (holder->info.id,
                                                            operation->window_set_boolean_parameter_suffix.value);
            break;
        }

        case OPERATION_TYPE_WINDOW_SET_KEYBOARD_GRAB:
        {
            struct window_info_holder_t *holder =
                KAN_HANDLE_GET (operation->window_set_boolean_parameter_suffix.window_handle);

            kan_platform_application_window_set_keyboard_grab (holder->info.id,
                                                               operation->window_set_boolean_parameter_suffix.value);
            break;
        }

        case OPERATION_TYPE_WINDOW_SET_OPACITY:
        {
            struct window_info_holder_t *holder =
                KAN_HANDLE_GET (operation->window_set_floating_point_parameter_suffix.window_handle);

            kan_platform_application_window_set_opacity (holder->info.id,
                                                         operation->window_set_floating_point_parameter_suffix.value);
            break;
        }

        case OPERATION_TYPE_WINDOW_SET_FOCUSABLE:
        {
            struct window_info_holder_t *holder =
                KAN_HANDLE_GET (operation->window_set_boolean_parameter_suffix.window_handle);

            kan_platform_application_window_set_focusable (holder->info.id,
                                                           operation->window_set_boolean_parameter_suffix.value);
            break;
        }

        case OPERATION_TYPE_WINDOW_ADD_TEXT_LISTENER:
        {
            struct window_info_holder_t *holder = KAN_HANDLE_GET (operation->window_parameterless_suffix.window_handle);
            ++holder->text_input_listeners;

            if (holder->text_input_listeners == 1u)
            {
                kan_platform_application_window_set_text_input_enabled (holder->info.id, true);
            }

            break;
        }

        case OPERATION_TYPE_WINDOW_REMOVE_TEXT_LISTENER:
        {
            struct window_info_holder_t *holder = KAN_HANDLE_GET (operation->window_parameterless_suffix.window_handle);
            KAN_ASSERT (holder->text_input_listeners > 0u)
            --holder->text_input_listeners;

            if (holder->text_input_listeners == 0u)
            {
                kan_platform_application_window_set_text_input_enabled (holder->info.id, false);
            }

            break;
        }

        case OPERATION_TYPE_WINDOW_ADD_RESOURCE:
        {
            struct window_info_holder_t *holder = KAN_HANDLE_GET (operation->window_add_resource_suffix.window_handle);

            struct window_resource_t *resource =
                kan_allocate_batched (system->window_infos_group, sizeof (struct window_resource_t));
            resource->id = operation->window_add_resource_suffix.resource_id;
            resource->binding = operation->window_add_resource_suffix.resource_binding;

            resource->next = holder->first_resource;
            resource->previous = NULL;

            if (holder->first_resource)
            {
                holder->first_resource->previous = resource;
            }

            holder->first_resource = resource;
            KAN_ASSERT (KAN_TYPED_ID_32_IS_VALID (holder->info.id))
            resource->binding.init (resource->binding.user_data, &holder->info);
            break;
        }

        case OPERATION_TYPE_WINDOW_REMOVE_RESOURCE:
        {
            struct window_info_holder_t *holder =
                KAN_HANDLE_GET (operation->window_remove_resource_suffix.window_handle);
            struct window_resource_t *resource = holder->first_resource;

            while (resource &&
                   KAN_TYPED_ID_32_IS_EQUAL (resource->id, operation->window_remove_resource_suffix.resource_id))
            {
                resource = resource->next;
            }

            KAN_ASSERT (resource)
            resource->binding.shutdown (resource->binding.user_data, &holder->info);

            if (resource->next)
            {
                resource->next->previous = resource->previous;
            }

            if (resource->previous)
            {
                resource->previous->next = resource->next;
            }
            else
            {
                KAN_ASSERT (resource == holder->first_resource)
                holder->first_resource = resource->next;
            }

            kan_free_batched (system->window_infos_group, resource);
            break;
        }

        case OPERATION_TYPE_WINDOW_DESTROY:
        {
            struct window_info_holder_t *holder = KAN_HANDLE_GET (operation->window_parameterless_suffix.window_handle);

            destroy_window_resources (system, holder);
            kan_platform_application_window_destroy (holder->info.id);

            if (holder->next)
            {
                holder->next->previous = holder->previous;
            }

            if (holder->previous)
            {
                holder->previous->next = holder->next;
            }
            else
            {
                KAN_ASSERT (system->first_window_info == holder)
                system->first_window_info = holder->next;
            }

            break;
        }

        case OPERATION_TYPE_WARP_MOUSE_GLOBAL:
            kan_platform_application_warp_mouse_global (operation->warp_mouse_global_suffix.global_x,
                                                        operation->warp_mouse_global_suffix.global_y);
            break;

        case OPERATION_TYPE_WARP_MOUSE_TO_WINDOW:
        {
            struct window_info_holder_t *holder = KAN_HANDLE_GET (operation->warp_mouse_to_window_suffix.window_handle);

            kan_platform_application_warp_mouse_in_window (holder->info.id,
                                                           operation->warp_mouse_to_window_suffix.local_x,
                                                           operation->warp_mouse_to_window_suffix.local_y);
            break;
        }

        case OPERATION_TYPE_SET_CURSOR_VISIBLE:
            kan_platform_application_set_cursor_visible (operation->set_cursor_visible_suffix.visible);
            break;

        case OPERATION_TYPE_CLIPBOARD_SET_TEXT:
            kan_platform_application_put_text_into_clipboard (operation->clipboard_set_text_suffix.text);
            break;
        }

        struct operation_t *next = operation->next;
        shutdown_operation (operation, system->operations_group);
        operation = next;
    }

    kan_stack_group_allocator_reset (&system->operation_temporary_allocator);
    system->first_operation = NULL;
    system->last_operation = NULL;
}

static inline void clean_and_pull_events (struct application_system_t *system, bool *needs_clipboard_update)
{
    struct event_node_t *event_node;
    while ((event_node = (struct event_node_t *) kan_event_queue_clean_oldest (&system->event_queue)))
    {
        free_event_node (event_node, system->events_group);
    }

    *needs_clipboard_update = !system->initial_clipboard_update_done;
    system->initial_clipboard_update_done = true;
    struct kan_platform_application_event_t event;

    while (kan_platform_application_fetch_next_event (&event))
    {
        struct event_node_t *node = (struct event_node_t *) kan_event_queue_submit_begin (&system->event_queue);
        if (node)
        {
            kan_platform_application_event_move (&event, &node->event);
            if (node->event.type == KAN_PLATFORM_APPLICATION_EVENT_TYPE_CLIPBOARD_UPDATE)
            {
                *needs_clipboard_update = true;
            }

            kan_event_queue_submit_end (&system->event_queue, &allocate_event_node (system->events_group)->node);
        }
        else
        {
            kan_platform_application_event_shutdown (&event);
        }
    }
}

static inline struct display_info_holder_t *allocate_empty_display_info_holder (struct application_system_t *system)
{
    struct display_info_holder_t *holder =
        kan_allocate_batched (system->display_infos_group, sizeof (struct display_info_holder_t));
    holder->next = NULL;

    kan_dynamic_array_init (&holder->info.fullscreen_modes, 0u, sizeof (struct kan_platform_display_mode_t),
                            alignof (struct kan_platform_display_mode_t), system->display_infos_group);
    return holder;
}

static inline void sync_info_and_clipboard (struct application_system_t *system, bool update_clipboard)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->sync_info_section);

    struct kan_dynamic_array_t display_ids;
    kan_dynamic_array_init (&display_ids, 0u, sizeof (kan_platform_display_id_t), alignof (kan_platform_display_id_t),
                            system->display_infos_group);

    kan_platform_application_get_display_ids (&display_ids);
    if (display_ids.size > 0u)
    {
        if (!system->first_display_info)
        {
            system->first_display_info = allocate_empty_display_info_holder (system);
        }

        struct display_info_holder_t *current_holder = system->first_display_info;
        for (kan_loop_size_t index = 0u; index < display_ids.size; ++index)
        {
            kan_platform_display_id_t display_id = ((kan_platform_display_id_t *) display_ids.data)[index];
            current_holder->info.id = display_id;
            kan_platform_application_get_display_bounds (display_id, &current_holder->info.bounds);
            current_holder->info.orientation = kan_platform_application_get_display_orientation (display_id);
            current_holder->info.content_scale = kan_platform_application_get_display_content_scale (display_id);

            current_holder->info.fullscreen_modes.size = 0u;
            kan_platform_application_get_fullscreen_display_modes (display_id, &current_holder->info.fullscreen_modes);
            kan_platform_application_get_current_display_mode (display_id, &current_holder->info.current_mode);
            kan_platform_application_get_desktop_display_mode (display_id, &current_holder->info.desktop_mode);

            if (index + 1u < display_ids.size)
            {
                if (!current_holder->next)
                {
                    current_holder->next = allocate_empty_display_info_holder (system);
                }

                current_holder = current_holder->next;
            }
        }

        if (current_holder->next)
        {
            application_system_clean_display_info_since (system, current_holder->next);
        }
    }
    else
    {
        application_system_clean_display_info_since (system, system->first_display_info);
        system->first_display_info = NULL;
    }

    kan_dynamic_array_shutdown (&display_ids);
    struct window_info_holder_t *window = system->first_window_info;

    while (window)
    {
        if (KAN_TYPED_ID_32_IS_VALID (window->info.id))
        {
            window->info.display_id = kan_platform_application_window_get_display_id (window->info.id);

            window->info.pixel_density = kan_platform_application_window_get_pixel_density (window->info.id);
            window->info.display_scale = kan_platform_application_window_get_display_scale (window->info.id);
            window->info.opacity = kan_platform_application_window_get_opacity (window->info.id);

            window->info.pixel_format = kan_platform_application_window_get_pixel_format (window->info.id);
            window->info.flags = kan_platform_application_window_get_flags (window->info.id);

            kan_platform_visual_offset_t position_x;
            kan_platform_visual_offset_t position_y;
            bool bounds_read = kan_platform_application_window_get_position (window->info.id, &position_x, &position_y);

            kan_platform_visual_size_t size_x;
            kan_platform_visual_size_t size_y;
            bounds_read &= kan_platform_application_window_get_size (window->info.id, &size_x, &size_y);

            if (bounds_read)
            {
                window->info.bounds.min_x = position_x;
                window->info.bounds.min_y = position_y;
                window->info.bounds.max_x = position_x + (kan_platform_visual_offset_t) size_x;
                window->info.bounds.max_y = position_y + (kan_platform_visual_offset_t) size_y;
            }

            if (!kan_platform_application_window_get_size_for_render (window->info.id, &window->info.width_for_render,
                                                                      &window->info.height_for_render))
            {
                window->info.width_for_render = 0u;
                window->info.height_for_render = 0u;
            }

            if (!kan_platform_application_window_get_minimum_size (window->info.id, &window->info.minimum_width,
                                                                   &window->info.minimum_height))
            {
                window->info.minimum_width = 0u;
                window->info.minimum_height = 0u;
            }

            if (!kan_platform_application_window_get_maximum_size (window->info.id, &window->info.maximum_width,
                                                                   &window->info.maximum_height))
            {
                window->info.maximum_width = 0u;
                window->info.maximum_height = 0u;
            }
        }

        window = window->next;
    }

    system->mouse_state.button_mask = kan_platform_application_get_mouse_state_local_to_focus (
        &system->mouse_state.local_x, &system->mouse_state.local_y);
    system->mouse_state.button_mask = kan_platform_application_get_mouse_state_local_to_focus (
        &system->mouse_state.global_x, &system->mouse_state.global_y);

    if (update_clipboard)
    {
        if (system->clipboard_content)
        {
            kan_free_general (system->clipboard_group, system->clipboard_content,
                              strlen (system->clipboard_content) + 1u);
        }

        char *extracted = kan_platform_application_extract_text_from_clipboard ();
        const kan_instance_size_t length = (kan_instance_size_t) strlen (extracted);
        system->clipboard_content = kan_allocate_general (system->clipboard_group, length + 1u, alignof (char));
        memcpy (system->clipboard_content, extracted, length + 1u);
        kan_free_general (kan_platform_application_get_clipboard_allocation_group (), extracted, length + 1u);
    }

    kan_cpu_section_execution_shutdown (&execution);
}

void kan_application_system_sync_in_main_thread (kan_context_system_t system_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    bool update_clipboard;

    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->sync_main_section);

    flush_operations (system);
    clean_and_pull_events (system, &update_clipboard);
    sync_info_and_clipboard (system, update_clipboard);
    kan_cpu_section_execution_shutdown (&execution);
}

void kan_application_system_prepare_for_destroy_in_main_thread (kan_context_system_t system_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    struct window_info_holder_t *holder = system->first_window_info;

    while (holder)
    {
        if (KAN_TYPED_ID_32_IS_VALID (holder->info.id))
        {
            destroy_window_resources (system, holder);
            kan_platform_application_window_destroy (holder->info.id);
            holder->info.id = KAN_TYPED_ID_32_SET_INVALID (kan_platform_window_id_t);
            holder->first_resource = NULL;
        }

        holder = holder->next;
    }
}

kan_application_system_event_iterator_t kan_application_system_event_iterator_create (
    kan_context_system_t system_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    return KAN_HANDLE_TRANSIT (kan_application_system_event_iterator_t,
                               kan_event_queue_iterator_create (&system->event_queue));
}

const struct kan_platform_application_event_t *kan_application_system_event_iterator_get (
    kan_context_system_t system_handle, kan_application_system_event_iterator_t event_iterator)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    struct event_node_t *node = (struct event_node_t *) kan_event_queue_iterator_get (
        &system->event_queue, KAN_HANDLE_TRANSIT (kan_event_queue_iterator_t, event_iterator));
    return node ? &node->event : NULL;
}

kan_application_system_event_iterator_t kan_application_system_event_iterator_advance (
    kan_application_system_event_iterator_t event_iterator)
{
    return KAN_HANDLE_TRANSIT (
        kan_application_system_event_iterator_t,
        kan_event_queue_iterator_advance (KAN_HANDLE_TRANSIT (kan_event_queue_iterator_t, event_iterator)));
}

void kan_application_system_event_iterator_destroy (kan_context_system_t system_handle,
                                                    kan_application_system_event_iterator_t event_iterator)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    kan_event_queue_iterator_destroy (&system->event_queue,
                                      KAN_HANDLE_TRANSIT (kan_event_queue_iterator_t, event_iterator));
}

kan_application_system_display_info_iterator_t kan_application_system_display_info_iterator_create (
    kan_context_system_t system_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    return KAN_HANDLE_SET (kan_application_system_display_info_iterator_t, system->first_display_info);
}

const struct kan_application_system_display_info_t *kan_application_system_display_info_iterator_get (
    kan_application_system_display_info_iterator_t display_info_iterator)
{
    struct display_info_holder_t *holder = KAN_HANDLE_GET (display_info_iterator);
    return holder ? &holder->info : NULL;
}

kan_application_system_display_info_iterator_t kan_application_system_display_info_iterator_advance (
    kan_application_system_display_info_iterator_t display_info_iterator)
{
    struct display_info_holder_t *holder = KAN_HANDLE_GET (display_info_iterator);
    return holder ? KAN_HANDLE_SET (kan_application_system_display_info_iterator_t, holder->next) :
                    KAN_HANDLE_SET_INVALID (kan_application_system_display_info_iterator_t);
}

kan_application_system_window_info_iterator_t kan_application_system_window_info_iterator_create (
    kan_context_system_t system_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    return KAN_HANDLE_SET (kan_application_system_window_info_iterator_t, system->first_window_info);
}

const struct kan_application_system_window_info_t *kan_application_system_window_info_iterator_get (
    kan_application_system_window_info_iterator_t window_info_iterator)
{
    struct window_info_holder_t *holder = KAN_HANDLE_GET (window_info_iterator);
    return holder ? &holder->info : NULL;
}

kan_application_system_window_info_iterator_t kan_application_system_window_info_iterator_advance (
    kan_application_system_window_info_iterator_t window_info_iterator)
{
    struct window_info_holder_t *holder = KAN_HANDLE_GET (window_info_iterator);
    return holder ? KAN_HANDLE_SET (kan_application_system_window_info_iterator_t, holder->next) :
                    KAN_HANDLE_SET_INVALID (kan_application_system_window_info_iterator_t);
}

const struct kan_application_system_window_info_t *kan_application_system_get_window_info_from_handle (
    kan_context_system_t system_handle, kan_application_system_window_t window_handle)
{
    struct window_info_holder_t *holder = KAN_HANDLE_GET (window_handle);
    KAN_ASSERT (!holder || KAN_HANDLE_IS_EQUAL (holder->info.handle, window_handle))
    return holder ? &holder->info : NULL;
}

static inline void insert_operation (struct application_system_t *system, struct operation_t *operation)
{
    operation->next = NULL;
    if (system->last_operation)
    {
        system->last_operation->next = operation;
        system->last_operation = operation;
    }
    else
    {
        system->first_operation = operation;
        system->last_operation = operation;
    }
}

kan_application_system_window_t kan_application_system_window_create (kan_context_system_t system_handle,
                                                                      const char *title,
                                                                      kan_platform_visual_size_t width,
                                                                      kan_platform_visual_size_t height,
                                                                      enum kan_platform_window_flag_t flags)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct window_info_holder_t *window_info_holder =
        kan_allocate_batched (system->window_infos_group, sizeof (struct window_info_holder_t));

    if (system->first_window_info)
    {
        system->first_window_info->previous = window_info_holder;
    }

    window_info_holder->info.handle = KAN_HANDLE_SET (kan_application_system_window_t, window_info_holder);
    window_info_holder->info.id = KAN_TYPED_ID_32_SET_INVALID (kan_platform_window_id_t);
    window_info_holder->text_input_listeners = 0u;
    window_info_holder->first_resource = NULL;
    window_info_holder->previous = NULL;
    window_info_holder->next = system->first_window_info;
    system->first_window_info = window_info_holder;

    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_CREATE;
    operation->window_create_suffix.window_handle = window_info_holder->info.handle;
    operation->window_create_suffix.width = width;
    operation->window_create_suffix.height = height;
    operation->window_create_suffix.flags = flags;

    const kan_instance_size_t title_length = (kan_instance_size_t) strlen (title);
    char *title_on_stack =
        kan_stack_group_allocator_allocate (&system->operation_temporary_allocator, title_length + 1u, alignof (char));
    memcpy (title_on_stack, title, title_length + 1u);
    operation->window_create_suffix.title = title_on_stack;

    insert_operation (system, operation);
    return window_info_holder->info.handle;
}

void kan_application_system_window_enter_fullscreen (kan_context_system_t system_handle,
                                                     kan_application_system_window_t window_handle,
                                                     const struct kan_platform_display_mode_t *display_mode)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_ENTER_FULLSCREEN;
    operation->window_enter_fullscreen_suffix.window_handle = window_handle;
    operation->window_enter_fullscreen_suffix.display_mode = display_mode;
    insert_operation (system, operation);
}

void kan_application_system_window_leave_fullscreen (kan_context_system_t system_handle,
                                                     kan_application_system_window_t window_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_LEAVE_FULLSCREEN;
    operation->window_parameterless_suffix.window_handle = window_handle;
    insert_operation (system, operation);
}

void kan_application_system_window_set_title (kan_context_system_t system_handle,
                                              kan_application_system_window_t window_handle,
                                              const char *title)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_SET_TITLE;
    operation->window_set_textual_parameter_suffix.window_handle = window_handle;

    const kan_instance_size_t title_length = (kan_instance_size_t) strlen (title);
    char *title_on_stack =
        kan_stack_group_allocator_allocate (&system->operation_temporary_allocator, title_length + 1u, alignof (char));
    memcpy (title_on_stack, title, title_length + 1u);

    operation->window_set_textual_parameter_suffix.data = title_on_stack;
    insert_operation (system, operation);
}

void kan_application_system_window_set_icon (kan_context_system_t system_handle,
                                             kan_application_system_window_t window_handle,
                                             enum kan_platform_pixel_format_t pixel_format,
                                             kan_platform_visual_size_t width,
                                             kan_platform_visual_size_t height,
                                             const void *data)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_SET_ICON;
    operation->window_set_icon_suffix.window_handle = window_handle;
    operation->window_set_icon_suffix.pixel_format = pixel_format;
    operation->window_set_icon_suffix.width = width;
    operation->window_set_icon_suffix.height = height;
    operation->window_set_icon_suffix.data = data;
    insert_operation (system, operation);
}

void kan_application_system_window_set_bounds (kan_context_system_t system_handle,
                                               kan_application_system_window_t window_handle,
                                               struct kan_platform_integer_bounds_t bounds)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_SET_BOUNDS;
    operation->window_set_bounds_suffix.window_handle = window_handle;
    operation->window_set_bounds_suffix.bounds = bounds;
    insert_operation (system, operation);
}

void kan_application_system_window_set_minimum_size (kan_context_system_t system_handle,
                                                     kan_application_system_window_t window_handle,
                                                     kan_platform_visual_size_t minimum_width,
                                                     kan_platform_visual_size_t minimum_height)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_SET_MINIMUM_SIZE;
    operation->window_set_size_limit_suffix.window_handle = window_handle;
    operation->window_set_size_limit_suffix.width = minimum_width;
    operation->window_set_size_limit_suffix.height = minimum_height;
    insert_operation (system, operation);
}

void kan_application_system_window_set_maximum_size (kan_context_system_t system_handle,
                                                     kan_application_system_window_t window_handle,
                                                     kan_platform_visual_size_t maximum_width,
                                                     kan_platform_visual_size_t maximum_height)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_SET_MAXIMUM_SIZE;
    operation->window_set_size_limit_suffix.window_handle = window_handle;
    operation->window_set_size_limit_suffix.width = maximum_width;
    operation->window_set_size_limit_suffix.height = maximum_height;
    insert_operation (system, operation);
}

void kan_application_system_window_set_bordered (kan_context_system_t system_handle,
                                                 kan_application_system_window_t window_handle,
                                                 bool bordered)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_SET_BORDERED;
    operation->window_set_boolean_parameter_suffix.window_handle = window_handle;
    operation->window_set_boolean_parameter_suffix.value = bordered;
    insert_operation (system, operation);
}

void kan_application_system_window_set_resizable (kan_context_system_t system_handle,
                                                  kan_application_system_window_t window_handle,
                                                  bool resizable)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_SET_RESIZEABLE;
    operation->window_set_boolean_parameter_suffix.window_handle = window_handle;
    operation->window_set_boolean_parameter_suffix.value = resizable;
    insert_operation (system, operation);
}

void kan_application_system_window_set_always_on_top (kan_context_system_t system_handle,
                                                      kan_application_system_window_t window_handle,
                                                      bool always_on_top)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_SET_ALWAYS_ON_TOP;
    operation->window_set_boolean_parameter_suffix.window_handle = window_handle;
    operation->window_set_boolean_parameter_suffix.value = always_on_top;
    insert_operation (system, operation);
}

void kan_application_system_window_show (kan_context_system_t system_handle,
                                         kan_application_system_window_t window_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_SHOW;
    operation->window_parameterless_suffix.window_handle = window_handle;
    insert_operation (system, operation);
}

void kan_application_system_window_hide (kan_context_system_t system_handle,
                                         kan_application_system_window_t window_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_HIDE;
    operation->window_parameterless_suffix.window_handle = window_handle;
    insert_operation (system, operation);
}

void kan_application_system_window_raise (kan_context_system_t system_handle,
                                          kan_application_system_window_t window_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_RAISE;
    operation->window_parameterless_suffix.window_handle = window_handle;
    insert_operation (system, operation);
}

void kan_application_system_window_minimize (kan_context_system_t system_handle,
                                             kan_application_system_window_t window_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_MINIMIZE;
    operation->window_parameterless_suffix.window_handle = window_handle;
    insert_operation (system, operation);
}

void kan_application_system_window_maximize (kan_context_system_t system_handle,
                                             kan_application_system_window_t window_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_MAXIMIZE;
    operation->window_parameterless_suffix.window_handle = window_handle;
    insert_operation (system, operation);
}

void kan_application_system_window_restore (kan_context_system_t system_handle,
                                            kan_application_system_window_t window_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_RESTORE;
    operation->window_parameterless_suffix.window_handle = window_handle;
    insert_operation (system, operation);
}

void kan_application_system_window_set_mouse_grab (kan_context_system_t system_handle,
                                                   kan_application_system_window_t window_handle,
                                                   bool mouse_grab)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_SET_MOUSE_GRAB;
    operation->window_set_boolean_parameter_suffix.window_handle = window_handle;
    operation->window_set_boolean_parameter_suffix.value = mouse_grab;
    insert_operation (system, operation);
}

void kan_application_system_window_set_keyboard_grab (kan_context_system_t system_handle,
                                                      kan_application_system_window_t window_handle,
                                                      bool keyboard_grab)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_SET_KEYBOARD_GRAB;
    operation->window_set_boolean_parameter_suffix.window_handle = window_handle;
    operation->window_set_boolean_parameter_suffix.value = keyboard_grab;
    insert_operation (system, operation);
}

void kan_application_system_window_set_opacity (kan_context_system_t system_handle,
                                                kan_application_system_window_t window_handle,
                                                float opacity)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_SET_OPACITY;
    operation->window_set_floating_point_parameter_suffix.window_handle = window_handle;
    operation->window_set_floating_point_parameter_suffix.value = opacity;
    insert_operation (system, operation);
}

void kan_application_window_set_focusable (kan_context_system_t system_handle,
                                           kan_application_system_window_t window_handle,
                                           bool focusable)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_SET_FOCUSABLE;
    operation->window_set_boolean_parameter_suffix.window_handle = window_handle;
    operation->window_set_boolean_parameter_suffix.value = focusable;
    insert_operation (system, operation);
}

void kan_application_window_add_text_listener (kan_context_system_t system_handle,
                                               kan_application_system_window_t window_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_ADD_TEXT_LISTENER;
    operation->window_parameterless_suffix.window_handle = window_handle;
    insert_operation (system, operation);
}

void kan_application_window_remove_text_listener (kan_context_system_t system_handle,
                                                  kan_application_system_window_t window_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_REMOVE_TEXT_LISTENER;
    operation->window_parameterless_suffix.window_handle = window_handle;
    insert_operation (system, operation);
}

kan_application_system_window_resource_id_t kan_application_system_window_add_resource (
    kan_context_system_t system_handle,
    kan_application_system_window_t window_handle,
    struct kan_application_system_window_resource_binding_t binding)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    kan_application_system_window_resource_id_t resource_id =
        KAN_TYPED_ID_32_SET (kan_application_system_window_resource_id_t,
                             (kan_id_32_t) kan_atomic_int_add (&system->resource_id_counter, 1));

    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_ADD_RESOURCE;
    operation->window_add_resource_suffix.window_handle = window_handle;
    operation->window_add_resource_suffix.resource_id = resource_id;
    operation->window_add_resource_suffix.resource_binding = binding;

    insert_operation (system, operation);
    return resource_id;
}

CONTEXT_APPLICATION_SYSTEM_API void kan_application_system_window_remove_resource (
    kan_context_system_t system_handle,
    kan_application_system_window_t window_handle,
    kan_application_system_window_resource_id_t resource_id)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_REMOVE_RESOURCE;
    operation->window_remove_resource_suffix.window_handle = window_handle;
    operation->window_remove_resource_suffix.resource_id = resource_id;

    insert_operation (system, operation);
}

void kan_application_system_window_destroy (kan_context_system_t system_handle,
                                            kan_application_system_window_t window_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WINDOW_DESTROY;
    operation->window_parameterless_suffix.window_handle = window_handle;
    insert_operation (system, operation);
}

const struct kan_application_system_mouse_state_t *kan_application_system_get_mouse_state (
    kan_context_system_t system_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    return &system->mouse_state;
}

void kan_application_system_warp_mouse_global (kan_context_system_t system_handle, float global_x, float global_y)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WARP_MOUSE_GLOBAL;
    operation->warp_mouse_global_suffix.global_x = global_x;
    operation->warp_mouse_global_suffix.global_y = global_y;
    insert_operation (system, operation);
}

void kan_application_system_warp_mouse_to_window (kan_context_system_t system_handle,
                                                  kan_application_system_window_t window_handle,
                                                  float local_x,
                                                  float local_y)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_WARP_MOUSE_TO_WINDOW;
    operation->warp_mouse_to_window_suffix.window_handle = window_handle;
    operation->warp_mouse_to_window_suffix.local_x = local_x;
    operation->warp_mouse_to_window_suffix.local_y = local_y;
    insert_operation (system, operation);
}

void kan_application_system_set_cursor_visible (kan_context_system_t system_handle, bool cursor_visible)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));

    operation->type = OPERATION_TYPE_SET_CURSOR_VISIBLE;
    operation->set_cursor_visible_suffix.visible = cursor_visible;
    insert_operation (system, operation);
}

const char *kan_application_system_clipboard_get_text (kan_context_system_t system_handle)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    return system->clipboard_content;
}

void kan_application_system_clipboard_set_text (kan_context_system_t system_handle, const char *text)
{
    struct application_system_t *system = KAN_HANDLE_GET (system_handle);
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->operation_submission_lock)
    struct operation_t *operation = kan_stack_group_allocator_allocate (
        &system->operation_temporary_allocator, sizeof (struct operation_t), alignof (struct operation_t));
    operation->type = OPERATION_TYPE_CLIPBOARD_SET_TEXT;

    const kan_instance_size_t text_length = (kan_instance_size_t) strlen (text);
    char *text_copied = kan_allocate_general (system->operations_group, text_length + 1u, alignof (char));
    memcpy (text_copied, text, text_length + 1u);
    operation->clipboard_set_text_suffix.text = text_copied;
    insert_operation (system, operation);
}
