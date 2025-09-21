#include <kan/file_system/entry.h>
#include <kan/file_system/stream.h>
#include <kan/file_system_watcher/watcher.h>
#include <kan/precise_time/precise_time.h>
#include <kan/testing/testing.h>

static bool write_text_file (const char *file, const char *content)
{
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (file, true);
    if (!stream || !kan_stream_is_writeable (stream))
    {
        return false;
    }

    const kan_instance_size_t content_length = (kan_instance_size_t) strlen (content);
    const bool result = stream->operations->write (stream, strlen (content), content) == content_length;

    stream->operations->close (stream);
    KAN_TEST_CHECK (kan_file_system_check_existence (file))
    return result;
}

static void update_watcher (kan_file_system_watcher_t watcher)
{
    // Need to sleep for some time as some filesystems like NTFS might not make new files available right away.
    kan_precise_time_sleep (100000000u);
    kan_file_system_watcher_mark_for_update (watcher);

    while (!kan_file_system_watcher_is_up_to_date (watcher))
    {
        kan_precise_time_sleep (10000000u);
    }
}

KAN_TEST_CASE (add_file)
{
    KAN_TEST_ASSERT (kan_file_system_make_directory ("test_directory"))
    KAN_TEST_ASSERT (kan_file_system_make_directory ("test_directory/watched"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/watched/test1.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/watched/test2.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/test3.txt", "Hello, world!"))

    kan_file_system_watcher_t watcher = kan_file_system_watcher_create ("test_directory/watched");
    kan_file_system_watcher_iterator_t iterator = kan_file_system_watcher_iterator_create (watcher);

    KAN_TEST_ASSERT (write_text_file ("test_directory/watched/test3.txt", "New file"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/test3.txt", "Should be ignored"))
    update_watcher (watcher);

    const struct kan_file_system_watcher_event_t *event = kan_file_system_watcher_iterator_get (watcher, iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->event_type == KAN_FILE_SYSTEM_EVENT_TYPE_ADDED)
    KAN_TEST_CHECK (event->entry_type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE)
    KAN_TEST_CHECK (strcmp (event->path_container.path, "test_directory/watched/test3.txt") == 0)

    // File addition might be reported as creation + modification, because file is created by stream open operation and
    // data is written by stream write operation and watcher might encounter state between these two operations.
    iterator = kan_file_system_watcher_iterator_advance (watcher, iterator);

    while ((event = kan_file_system_watcher_iterator_get (watcher, iterator)))
    {
        KAN_TEST_CHECK (event->event_type == KAN_FILE_SYSTEM_EVENT_TYPE_MODIFIED)
        KAN_TEST_CHECK (event->entry_type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE)
        KAN_TEST_CHECK (strcmp (event->path_container.path, "test_directory/watched/test3.txt") == 0)
        iterator = kan_file_system_watcher_iterator_advance (watcher, iterator);
    }

    kan_file_system_watcher_iterator_destroy (watcher, iterator);
    kan_file_system_watcher_destroy (watcher);
    KAN_TEST_CHECK (kan_file_system_remove_directory_with_content ("test_directory"))
}

KAN_TEST_CASE (modify_file)
{
    KAN_TEST_ASSERT (kan_file_system_make_directory ("test_directory"))
    KAN_TEST_ASSERT (kan_file_system_make_directory ("test_directory/watched"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/watched/test1.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/watched/test2.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/test3.txt", "Hello, world!"))

    kan_file_system_watcher_t watcher = kan_file_system_watcher_create ("test_directory/watched");
    kan_file_system_watcher_iterator_t iterator = kan_file_system_watcher_iterator_create (watcher);

    KAN_TEST_ASSERT (write_text_file ("test_directory/watched/test2.txt", "New content"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/test3.txt", "Should be ignored"))
    update_watcher (watcher);

    const struct kan_file_system_watcher_event_t *event = kan_file_system_watcher_iterator_get (watcher, iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->event_type == KAN_FILE_SYSTEM_EVENT_TYPE_MODIFIED)
    KAN_TEST_CHECK (event->entry_type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE)
    KAN_TEST_CHECK (strcmp (event->path_container.path, "test_directory/watched/test2.txt") == 0)

    // File write might be processed as several operations in some cases and watcher might observe state between two
    // writes. Therefore, we need to be able to correctly process several modification events.
    iterator = kan_file_system_watcher_iterator_advance (watcher, iterator);

    while ((event = kan_file_system_watcher_iterator_get (watcher, iterator)))
    {
        KAN_TEST_CHECK (event->event_type == KAN_FILE_SYSTEM_EVENT_TYPE_MODIFIED)
        KAN_TEST_CHECK (event->entry_type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE)
        KAN_TEST_CHECK (strcmp (event->path_container.path, "test_directory/watched/test2.txt") == 0)
        iterator = kan_file_system_watcher_iterator_advance (watcher, iterator);
    }

    kan_file_system_watcher_iterator_destroy (watcher, iterator);
    kan_file_system_watcher_destroy (watcher);
    KAN_TEST_CHECK (kan_file_system_remove_directory_with_content ("test_directory"))
}

KAN_TEST_CASE (delete_file)
{
    KAN_TEST_ASSERT (kan_file_system_make_directory ("test_directory"))
    KAN_TEST_ASSERT (kan_file_system_make_directory ("test_directory/watched"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/watched/test1.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/watched/test2.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/test3.txt", "Hello, world!"))

    kan_file_system_watcher_t watcher = kan_file_system_watcher_create ("test_directory/watched");
    kan_file_system_watcher_iterator_t iterator = kan_file_system_watcher_iterator_create (watcher);

    KAN_TEST_ASSERT (kan_file_system_remove_file ("test_directory/watched/test2.txt"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/test3.txt", "Should be ignored"))
    update_watcher (watcher);

    const struct kan_file_system_watcher_event_t *event = kan_file_system_watcher_iterator_get (watcher, iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->event_type == KAN_FILE_SYSTEM_EVENT_TYPE_REMOVED)
    KAN_TEST_CHECK (event->entry_type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE)
    KAN_TEST_CHECK (strcmp (event->path_container.path, "test_directory/watched/test2.txt") == 0)

    iterator = kan_file_system_watcher_iterator_advance (watcher, iterator);
    KAN_TEST_CHECK (!kan_file_system_watcher_iterator_get (watcher, iterator))

    kan_file_system_watcher_iterator_destroy (watcher, iterator);
    kan_file_system_watcher_destroy (watcher);
    KAN_TEST_CHECK (kan_file_system_remove_directory_with_content ("test_directory"))
}

KAN_TEST_CASE (add_directory)
{
    KAN_TEST_ASSERT (kan_file_system_make_directory ("test_directory"))
    KAN_TEST_ASSERT (kan_file_system_make_directory ("test_directory/watched"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/watched/test1.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/watched/test2.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/test3.txt", "Hello, world!"))

    kan_file_system_watcher_t watcher = kan_file_system_watcher_create ("test_directory/watched");
    kan_file_system_watcher_iterator_t iterator = kan_file_system_watcher_iterator_create (watcher);

    KAN_TEST_ASSERT (kan_file_system_make_directory ("test_directory/watched/sub"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/watched/sub/1.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/watched/sub/2.txt", "Hello, world!"))
    update_watcher (watcher);

    bool add_sub_found = false;
    bool add_sub_1_found = false;
    bool add_sub_2_found = false;

    // We need to be cautious about the case when file creation is read as addition+modification.
    const struct kan_file_system_watcher_event_t *event;

    while ((event = kan_file_system_watcher_iterator_get (watcher, iterator)))
    {
        if (strcmp (event->path_container.path, "test_directory/watched/sub") == 0)
        {
            KAN_TEST_CHECK (!add_sub_found)
            KAN_TEST_CHECK (event->event_type == KAN_FILE_SYSTEM_EVENT_TYPE_ADDED)
            KAN_TEST_CHECK (event->entry_type == KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY)
            add_sub_found = true;
        }
        else if (strcmp (event->path_container.path, "test_directory/watched/sub/1.txt") == 0)
        {
            if (add_sub_1_found)
            {
                KAN_TEST_CHECK (event->event_type == KAN_FILE_SYSTEM_EVENT_TYPE_MODIFIED)
                KAN_TEST_CHECK (event->entry_type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE)
            }
            else
            {
                KAN_TEST_CHECK (event->event_type == KAN_FILE_SYSTEM_EVENT_TYPE_ADDED)
                KAN_TEST_CHECK (event->entry_type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE)
                add_sub_1_found = true;
            }
        }
        else if (strcmp (event->path_container.path, "test_directory/watched/sub/2.txt") == 0)
        {
            if (add_sub_2_found)
            {
                KAN_TEST_CHECK (event->event_type == KAN_FILE_SYSTEM_EVENT_TYPE_MODIFIED)
                KAN_TEST_CHECK (event->entry_type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE)
            }
            else
            {
                KAN_TEST_CHECK (event->event_type == KAN_FILE_SYSTEM_EVENT_TYPE_ADDED)
                KAN_TEST_CHECK (event->entry_type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE)
                add_sub_2_found = true;
            }
        }
        else
        {
            // Unexpected event.
            KAN_TEST_CHECK (false)
        }

        iterator = kan_file_system_watcher_iterator_advance (watcher, iterator);
    }

    KAN_TEST_CHECK (add_sub_found)
    KAN_TEST_CHECK (add_sub_1_found)
    KAN_TEST_CHECK (add_sub_2_found)

    kan_file_system_watcher_iterator_destroy (watcher, iterator);
    kan_file_system_watcher_destroy (watcher);
    KAN_TEST_CHECK (kan_file_system_remove_directory_with_content ("test_directory"))
}

KAN_TEST_CASE (remove_directory)
{
    KAN_TEST_ASSERT (kan_file_system_make_directory ("test_directory"))
    KAN_TEST_ASSERT (kan_file_system_make_directory ("test_directory/watched"))
    KAN_TEST_ASSERT (kan_file_system_make_directory ("test_directory/watched/sub"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/watched/test1.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/watched/test2.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/watched/sub/1.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/watched/sub/2.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file ("test_directory/test3.txt", "Hello, world!"))

    kan_file_system_watcher_t watcher = kan_file_system_watcher_create ("test_directory/watched");
    kan_file_system_watcher_iterator_t iterator = kan_file_system_watcher_iterator_create (watcher);

    KAN_TEST_ASSERT (kan_file_system_remove_directory_with_content ("test_directory/watched/sub"))
    update_watcher (watcher);

    bool remove_sub_found = false;
    bool remove_sub_1_found = false;
    bool remove_sub_2_found = false;

    const struct kan_file_system_watcher_event_t *event;
    while ((event = kan_file_system_watcher_iterator_get (watcher, iterator)))
    {
        KAN_TEST_CHECK (!remove_sub_found || !remove_sub_1_found || !remove_sub_2_found)

        if (strcmp (event->path_container.path, "test_directory/watched/sub") == 0)
        {
            KAN_TEST_CHECK (!remove_sub_found)
            KAN_TEST_CHECK (event->event_type == KAN_FILE_SYSTEM_EVENT_TYPE_REMOVED)
            KAN_TEST_CHECK (event->entry_type == KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY)
            remove_sub_found = true;
        }
        else if (strcmp (event->path_container.path, "test_directory/watched/sub/1.txt") == 0)
        {
            KAN_TEST_CHECK (!remove_sub_1_found)
            KAN_TEST_CHECK (event->event_type == KAN_FILE_SYSTEM_EVENT_TYPE_REMOVED)
            KAN_TEST_CHECK (event->entry_type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE)
            remove_sub_1_found = true;
        }
        else if (strcmp (event->path_container.path, "test_directory/watched/sub/2.txt") == 0)
        {
            KAN_TEST_CHECK (!remove_sub_2_found)
            KAN_TEST_CHECK (event->event_type == KAN_FILE_SYSTEM_EVENT_TYPE_REMOVED)
            KAN_TEST_CHECK (event->entry_type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE)
            remove_sub_2_found = true;
        }
        else
        {
            // Unexpected event.
            KAN_TEST_CHECK (false)
        }

        iterator = kan_file_system_watcher_iterator_advance (watcher, iterator);
    }

    KAN_TEST_CHECK (remove_sub_found)
    KAN_TEST_CHECK (remove_sub_1_found)
    KAN_TEST_CHECK (remove_sub_2_found)

    kan_file_system_watcher_iterator_destroy (watcher, iterator);
    kan_file_system_watcher_destroy (watcher);
    KAN_TEST_CHECK (kan_file_system_remove_directory_with_content ("test_directory"))
}
