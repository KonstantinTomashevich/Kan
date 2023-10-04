#include <stdlib.h>

#include <debugbreak.h>

#include <SDL_messagebox.h>

#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/threading/mutex.h>

KAN_LOG_DEFINE_CATEGORY (error);

struct critical_error_context_t
{
    kan_bool_t is_interactive;
    kan_mutex_handle_t interactive_mutex;
};

static kan_bool_t critical_error_context_ready = KAN_FALSE;
static struct critical_error_context_t critical_error_context;

static inline void kan_critical_error_context_ensure ()
{
    if (!critical_error_context_ready)
    {
        critical_error_context.is_interactive = KAN_FALSE;
        critical_error_context.interactive_mutex = kan_mutex_create ();
        critical_error_context_ready = KAN_TRUE;
    }
}

void kan_set_critical_error_interactive (kan_bool_t is_interactive)
{
    kan_critical_error_context_ensure ();
    critical_error_context.is_interactive = is_interactive;
}

void kan_critical_error (const char *message, const char *file, int line)
{
    kan_critical_error_context_ensure ();
    KAN_LOG (error, KAN_LOG_CRITICAL_ERROR, "Critical error: \"%s\". File: \"%s\". Line: \"%d\".", message, file, line)
    // TODO: Perhaps add stacktrace in future.

    if (critical_error_context.is_interactive)
    {
        kan_mutex_lock (critical_error_context.interactive_mutex);

        // TODO: Support for skip all feature.

#define BUTTON_SKIP 0
#define BUTTON_SKIP_ALL_OCCURRENCES 1
#define BUTTON_BREAK_INTO_DEBUGGER 2
#define BUTTON_ABORT 3

        const SDL_MessageBoxButtonData buttons[] = {
            {0, BUTTON_ABORT, "Abort"},
            {0, BUTTON_BREAK_INTO_DEBUGGER, "Debug"},
            {0, BUTTON_SKIP_ALL_OCCURRENCES, "Skip all"},
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, BUTTON_SKIP, "Skip"},
        };

        char box_message[KAN_ERROR_INTERACTIVE_CRITICAL_MESSAGE_MAX_LENGTH];
        snprintf (box_message, KAN_ERROR_INTERACTIVE_CRITICAL_MESSAGE_MAX_LENGTH,
                  "Critical error: \"%s\". File: \"%s\". Line: \"%d\".", message, file, line);

        const SDL_MessageBoxData message_box_data = {
            SDL_MESSAGEBOX_ERROR, NULL, "Assert failed!", box_message, SDL_arraysize (buttons), buttons, NULL};

        int result_button_id;
        int message_box_return_code = SDL_ShowMessageBox (&message_box_data, &result_button_id);
        kan_mutex_unlock (critical_error_context.interactive_mutex);

        if (message_box_return_code < 0)
        {
            KAN_LOG (testing, KAN_LOG_CRITICAL_ERROR, "Failed to create interactive assert message box: %s.",
                     SDL_GetError ())
            abort ();
        }

        // If window was closed.
        if (result_button_id < 0)
        {
            result_button_id = BUTTON_SKIP;
        }

        if (result_button_id == BUTTON_SKIP)
        {
            return;
        }

        if (result_button_id == BUTTON_SKIP_ALL_OCCURRENCES)
        {
            // TODO: Support for skip all feature.
            return;
        }

        if (result_button_id == BUTTON_BREAK_INTO_DEBUGGER)
        {
            debug_break ();
            return;
        }

        if (result_button_id == BUTTON_ABORT)
        {
            abort ();
        }

        KAN_LOG (testing, KAN_LOG_CRITICAL_ERROR, "Received unknown button from interactive assert message box.")
        abort ();
    }
    else
    {
        // No interactive: just crash.
        abort ();
    }
}
