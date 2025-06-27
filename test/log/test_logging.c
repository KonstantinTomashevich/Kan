#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <string.h>

#include <kan/log/logging.h>
#include <kan/log/observation.h>
#include <kan/testing/testing.h>

KAN_LOG_DEFINE_CATEGORY (test_log);

#define BUFFER_SIZE 1024u

static kan_instance_size_t callback_calls = 0u;
static kan_log_category_t callback_category;
static enum kan_log_verbosity_t callback_verbosity;
static char callback_message[BUFFER_SIZE];
static kan_functor_user_data_t callback_user_data;

static void test_callback (kan_log_category_t category,
                           enum kan_log_verbosity_t verbosity,
                           struct timespec time,
                           const char *message,
                           kan_functor_user_data_t user_data)
{
    ++callback_calls;
    callback_category = category;
    callback_verbosity = verbosity;
    strncpy (callback_message, message, BUFFER_SIZE - 1u);
    callback_user_data = user_data;
}

KAN_TEST_CASE (callback)
{
    const kan_log_category_t test_category = kan_log_category_get ("test_log");
    KAN_LOG (test_log, KAN_LOG_DEFAULT, "Hello, world!")
    kan_log_callback_add (test_callback, 0u);

    KAN_LOG (test_log, KAN_LOG_DEFAULT, "Hello, world!")
    KAN_TEST_CHECK (callback_calls == 1u)
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (callback_category, test_category))
    KAN_TEST_CHECK (callback_verbosity == KAN_LOG_DEFAULT)
    KAN_TEST_CHECK (strcmp (callback_message, "Hello, world!") == 0)
    KAN_TEST_CHECK (callback_user_data == 0u)

    static char test_message_buffer[BUFFER_SIZE];
    snprintf (test_message_buffer, BUFFER_SIZE, "Test formatting \"%s\" %d", "string", 42);
    KAN_LOG (test_log, KAN_LOG_DEFAULT, "Test formatting \"%s\" %d", "string", 42)
    KAN_TEST_CHECK (callback_calls == 2u)
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (callback_category, test_category))
    KAN_TEST_CHECK (callback_verbosity == KAN_LOG_DEFAULT)
    KAN_TEST_CHECK (strcmp (callback_message, test_message_buffer) == 0)
    KAN_TEST_CHECK (callback_user_data == 0u)

    kan_log_callback_remove (test_callback, 0u);
    KAN_LOG (test_log, KAN_LOG_DEFAULT, "Hello, world!")
    KAN_TEST_CHECK (callback_calls == 2u)
}

KAN_TEST_CASE (events)
{
    const kan_log_category_t test_category = kan_log_category_get ("test_log");
    KAN_LOG (test_log, KAN_LOG_DEFAULT, "Hello, world!")

    kan_log_event_iterator_t first_iterator = kan_log_event_iterator_create ();
    const struct kan_log_event_t *event = kan_log_event_iterator_get (first_iterator);
    KAN_TEST_CHECK (!event)

    KAN_LOG (test_log, KAN_LOG_DEFAULT, "Hello, world!")
    static char test_message_buffer[BUFFER_SIZE];
    snprintf (test_message_buffer, BUFFER_SIZE, "Test formatting \"%s\" %d", "string", 42);
    KAN_LOG (test_log, KAN_LOG_DEFAULT, "Test formatting \"%s\" %d", "string", 42)

    kan_log_event_iterator_t second_iterator = kan_log_event_iterator_create ();

    event = kan_log_event_iterator_get (first_iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (event->category, test_category))
    KAN_TEST_CHECK (event->verbosity == KAN_LOG_DEFAULT)
    KAN_TEST_CHECK (strcmp (event->message, "Hello, world!") == 0)

    first_iterator = kan_log_event_iterator_advance (first_iterator);
    event = kan_log_event_iterator_get (first_iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (event->category, test_category))
    KAN_TEST_CHECK (event->verbosity == KAN_LOG_DEFAULT)
    KAN_TEST_CHECK (strcmp (event->message, test_message_buffer) == 0)

    first_iterator = kan_log_event_iterator_advance (first_iterator);
    event = kan_log_event_iterator_get (first_iterator);
    KAN_TEST_CHECK (!event)

    second_iterator = kan_log_event_iterator_advance (second_iterator);
    event = kan_log_event_iterator_get (second_iterator);
    KAN_TEST_CHECK (!event)

    kan_log_event_iterator_destroy (first_iterator);
    KAN_LOG (test_log, KAN_LOG_DEFAULT, "Hello, world!")

    event = kan_log_event_iterator_get (second_iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (event->category, test_category))
    KAN_TEST_CHECK (event->verbosity == KAN_LOG_DEFAULT)
    KAN_TEST_CHECK (strcmp (event->message, "Hello, world!") == 0)

    second_iterator = kan_log_event_iterator_advance (second_iterator);
    event = kan_log_event_iterator_get (second_iterator);
    KAN_TEST_CHECK (!event)

    kan_log_event_iterator_destroy (second_iterator);

    kan_log_event_iterator_t not_reading_iterator = kan_log_event_iterator_create ();
    KAN_LOG (test_log, KAN_LOG_DEFAULT, "Hello, world!")
    KAN_LOG (test_log, KAN_LOG_DEFAULT, "Test formatting \"%s\" %d", "string", 42)
    KAN_LOG (test_log, KAN_LOG_DEFAULT, "Hello, world!")
    kan_log_event_iterator_destroy (not_reading_iterator);
}

KAN_TEST_CASE (verbosity)
{
    const kan_log_category_t test_category = kan_log_category_get ("test_log");
    kan_log_event_iterator_t iterator = kan_log_event_iterator_create ();

    KAN_LOG (test_log, KAN_LOG_VERBOSE, "Shouldn't be seen!")
    KAN_LOG (test_log, KAN_LOG_DEBUG, "Shouldn't be seen!")
    KAN_LOG (test_log, KAN_LOG_ERROR, "See me!")
    KAN_LOG (test_log, KAN_LOG_WARNING, "And me!")

    kan_log_category_set_verbosity (test_category, KAN_LOG_VERBOSE);
    KAN_LOG (test_log, KAN_LOG_VERBOSE, "Now everything is visible!")

    kan_log_category_set_verbosity (test_category, KAN_LOG_CRITICAL_ERROR);
    KAN_LOG (test_log, KAN_LOG_ERROR, "Now everything is hidden!")

    kan_log_category_set_verbosity (test_category, KAN_LOG_INFO);
    KAN_LOG (test_log, KAN_LOG_INFO, "Hello, world!")

    const struct kan_log_event_t *event = kan_log_event_iterator_get (iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (event->category, test_category))
    KAN_TEST_CHECK (event->verbosity == KAN_LOG_ERROR)
    KAN_TEST_CHECK (strcmp (event->message, "See me!") == 0)

    iterator = kan_log_event_iterator_advance (iterator);
    event = kan_log_event_iterator_get (iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (event->category, test_category))
    KAN_TEST_CHECK (event->verbosity == KAN_LOG_WARNING)
    KAN_TEST_CHECK (strcmp (event->message, "And me!") == 0)

    iterator = kan_log_event_iterator_advance (iterator);
    event = kan_log_event_iterator_get (iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (event->category, test_category))
    KAN_TEST_CHECK (event->verbosity == KAN_LOG_VERBOSE)
    KAN_TEST_CHECK (strcmp (event->message, "Now everything is visible!") == 0)

    iterator = kan_log_event_iterator_advance (iterator);
    event = kan_log_event_iterator_get (iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (event->category, test_category))
    KAN_TEST_CHECK (event->verbosity == KAN_LOG_INFO)
    KAN_TEST_CHECK (strcmp (event->message, "Hello, world!") == 0)

    iterator = kan_log_event_iterator_advance (iterator);
    event = kan_log_event_iterator_get (iterator);
    KAN_TEST_CHECK (!event)
}
