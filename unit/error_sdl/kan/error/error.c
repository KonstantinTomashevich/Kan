#include <stdlib.h>
#include <string.h>

#include <debugbreak.h>

#include <kan/api_common/mute_warnings.h>
KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#include <SDL3/SDL_messagebox.h>
KAN_MUTE_THIRD_PARTY_WARNINGS_END

#include <kan/container/hash_storage.h>
#include <kan/error/context.h>
#include <kan/error/critical.h>
#include <kan/hash/hash.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/threading/mutex.h>

KAN_LOG_DEFINE_CATEGORY (error_reporting);

struct critical_error_context_t
{
    bool is_interactive;
    kan_mutex_t interactive_mutex;
    struct kan_hash_storage_t skipped_error_storage;
};

struct skipped_error_node_t
{
    struct kan_hash_storage_node_t node;
    const char *file;
    int line;
};

static bool error_context_ready = false;
static struct critical_error_context_t critical_error_context;

#define SKIPPED_CRITICAL_ERROR_INFO_STORAGE_INITIAL_BUCKETS 8u

static inline void kan_error_context_ensure_initialized (void)
{
    if (!error_context_ready)
    {
        critical_error_context.is_interactive = false;
        critical_error_context.interactive_mutex = kan_mutex_create ();
        kan_hash_storage_init (&critical_error_context.skipped_error_storage, KAN_ALLOCATION_GROUP_IGNORE,
                               SKIPPED_CRITICAL_ERROR_INFO_STORAGE_INITIAL_BUCKETS);

        // We need to log at least once in order to initialize logging and prevent deadlock when
        // logging is initialized from critical error that was caught inside memory profiler.
        kan_log_ensure_initialized ();
        error_context_ready = true;
    }
}

static inline kan_hash_t hash_error (const char *file, int line)
{
    return kan_hash_combine (kan_string_hash (file), (kan_hash_t) line);
}

void kan_error_initialize (void)
{
    kan_error_context_ensure_initialized ();
    // We do not have crash handling yet, but it should be initialized here in the future.
}

void kan_error_set_critical_interactive (bool is_interactive)
{
    kan_error_context_ensure_initialized ();
    critical_error_context.is_interactive = is_interactive;
}

void kan_error_critical (const char *message, const char *file, int line)
{
    kan_error_context_ensure_initialized ();
    KAN_LOG (error_reporting, KAN_LOG_CRITICAL_ERROR, "Critical error: \"%s\". File: \"%s\". Line: \"%d\".", message,
             file, line)
    // TODO: Perhaps add stacktrace in future.

    if (critical_error_context.is_interactive)
    {
        kan_mutex_lock (critical_error_context.interactive_mutex);

        const kan_hash_t error_hash = hash_error (file, line);
        const struct kan_hash_storage_bucket_t *bucket =
            kan_hash_storage_query (&critical_error_context.skipped_error_storage, error_hash);

        struct skipped_error_node_t *node = (struct skipped_error_node_t *) bucket->first;
        const struct skipped_error_node_t *end =
            (struct skipped_error_node_t *) (bucket->last ? bucket->last->next : NULL);

        while (node != end)
        {
            if (node->node.hash == error_hash && node->line == line && strcmp (node->file, file) == 0)
            {
                kan_mutex_unlock (critical_error_context.interactive_mutex);
                return;
            }

            node = (struct skipped_error_node_t *) node->node.list_node.next;
        }

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

        if (message_box_return_code < 0)
        {
            KAN_LOG (error_reporting, KAN_LOG_CRITICAL_ERROR, "Failed to create interactive assert message box: %s.",
                     SDL_GetError ())
            kan_mutex_unlock (critical_error_context.interactive_mutex);
            abort ();
        }

        if (result_button_id == BUTTON_SKIP_ALL_OCCURRENCES)
        {
            node = kan_allocate_batched (KAN_ALLOCATION_GROUP_IGNORE, sizeof (struct skipped_error_node_t));
            node->node.hash = error_hash;
            node->file = file;
            node->line = line;

            kan_hash_storage_update_bucket_count_default (&critical_error_context.skipped_error_storage,
                                                          SKIPPED_CRITICAL_ERROR_INFO_STORAGE_INITIAL_BUCKETS);
            kan_hash_storage_add (&critical_error_context.skipped_error_storage, &node->node);

            kan_mutex_unlock (critical_error_context.interactive_mutex);
            return;
        }

        kan_mutex_unlock (critical_error_context.interactive_mutex);

        // If window was closed.
        if (result_button_id < 0)
        {
            result_button_id = BUTTON_SKIP;
        }

        if (result_button_id == BUTTON_SKIP)
        {
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

        KAN_LOG (error_reporting, KAN_LOG_CRITICAL_ERROR,
                 "Received unknown button from interactive assert message box.")
        abort ();
    }
    else
    {
        // No interactive: just crash.
        abort ();
    }
}
