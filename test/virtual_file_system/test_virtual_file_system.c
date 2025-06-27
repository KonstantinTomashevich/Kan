#include <string.h>

#include <kan/memory/allocation.h>
#include <kan/precise_time/precise_time.h>
#include <kan/stream/stream.h>
#include <kan/testing/testing.h>
#include <kan/virtual_file_system/virtual_file_system.h>

static bool write_text_file (kan_virtual_file_system_volume_t volume, const char *file, const char *content)
{
    struct kan_stream_t *stream = kan_virtual_file_stream_open_for_write (volume, file);
    if (!stream || !kan_stream_is_writeable (stream))
    {
        return false;
    }

    const kan_instance_size_t content_length = (kan_instance_size_t) strlen (content);
    const bool result = stream->operations->write (stream, strlen (content), content) == content_length;

    stream->operations->close (stream);
    KAN_TEST_CHECK (kan_virtual_file_system_check_existence (volume, file))
    return result;
}

static bool read_text_file (kan_virtual_file_system_volume_t volume, const char *file, const char *expected_content)
{
    struct kan_stream_t *stream = kan_virtual_file_stream_open_for_read (volume, file);
    if (!stream || !kan_stream_is_readable (stream))
    {
        return false;
    }

    KAN_TEST_ASSERT (stream->operations->seek (stream, KAN_STREAM_SEEK_END, 0))
    const kan_file_size_t file_length = stream->operations->tell (stream);
    KAN_TEST_ASSERT (file_length > 0u)
    KAN_TEST_ASSERT (stream->operations->seek (stream, KAN_STREAM_SEEK_START, 0))

    char *content = (char *) kan_allocate_general (KAN_ALLOCATION_GROUP_IGNORE, file_length + 1u, _Alignof (char));
    content[file_length] = '\0';
    KAN_TEST_ASSERT (stream->operations->read (stream, file_length, content) == file_length)
    const bool result = strcmp (content, expected_content) == 0;

    stream->operations->close (stream);
    KAN_TEST_CHECK (kan_virtual_file_system_check_existence (volume, file))
    kan_free_general (KAN_ALLOCATION_GROUP_IGNORE, content, file_length + 1u);
    return result;
}

static void give_some_time_for_poll (void)
{
    kan_precise_time_sleep (300000000u); // 300ms
}

KAN_TEST_CASE (create_and_remove_empty_directory)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    KAN_TEST_CHECK (kan_virtual_file_system_make_directory (volume, "test/some_directory"))
    KAN_TEST_CHECK (kan_virtual_file_system_check_existence (volume, "test/some_directory"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_empty_directory (volume, "test/some_directory"))
    KAN_TEST_CHECK (!kan_virtual_file_system_check_existence (volume, "test/some_directory"))
    kan_virtual_file_system_volume_destroy (volume);
}

KAN_TEST_CASE (create_and_remove_empty_directory_real)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    kan_virtual_file_system_volume_mount_real (volume, "test", ".");
    KAN_TEST_CHECK (kan_virtual_file_system_make_directory (volume, "test/some_directory"))
    KAN_TEST_CHECK (kan_virtual_file_system_check_existence (volume, "test/some_directory"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_empty_directory (volume, "test/some_directory"))
    KAN_TEST_CHECK (!kan_virtual_file_system_check_existence (volume, "test/some_directory"))
    kan_virtual_file_system_volume_destroy (volume);
}

KAN_TEST_CASE (create_and_remove_two_level_directories)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    KAN_TEST_CHECK (kan_virtual_file_system_make_directory (volume, "test/dir1/dir2"))
    KAN_TEST_CHECK (kan_virtual_file_system_check_existence (volume, "test/dir1/dir2"))
    KAN_TEST_CHECK (!kan_virtual_file_system_remove_empty_directory (volume, "test/dir1"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_empty_directory (volume, "test/dir1/dir2"))
    KAN_TEST_CHECK (kan_virtual_file_system_check_existence (volume, "test/dir1"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_empty_directory (volume, "test/dir1"))
    KAN_TEST_CHECK (!kan_virtual_file_system_check_existence (volume, "test/dir1"))
    kan_virtual_file_system_volume_destroy (volume);
}

KAN_TEST_CASE (create_and_remove_file_real)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    kan_virtual_file_system_volume_mount_real (volume, "test", ".");
    KAN_TEST_CHECK (write_text_file (volume, "test/test.txt", "Hello, world!"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "test/test.txt"))
    KAN_TEST_CHECK (!kan_virtual_file_system_check_existence (volume, "test.txt"))
    kan_virtual_file_system_volume_destroy (volume);
}

KAN_TEST_CASE (create_and_remove_directory_with_sub_directories)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    KAN_TEST_CHECK (kan_virtual_file_system_make_directory (volume, "test/dir1/dir_x"))
    KAN_TEST_CHECK (kan_virtual_file_system_make_directory (volume, "test/dir1/dir_y"))
    KAN_TEST_CHECK (kan_virtual_file_system_check_existence (volume, "test/dir1/dir_x"))
    KAN_TEST_CHECK (kan_virtual_file_system_check_existence (volume, "test/dir1/dir_y"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_directory_with_content (volume, "test/dir1"))
    KAN_TEST_CHECK (!kan_virtual_file_system_check_existence (volume, "test/dir1/dir_x"))
    KAN_TEST_CHECK (!kan_virtual_file_system_check_existence (volume, "test/dir1/dir_y"))
    KAN_TEST_CHECK (!kan_virtual_file_system_check_existence (volume, "test/dir1"))
    kan_virtual_file_system_volume_destroy (volume);
}

KAN_TEST_CASE (create_and_remove_directory_with_files)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    KAN_TEST_CHECK (kan_virtual_file_system_make_directory (volume, "test"))
    kan_virtual_file_system_volume_mount_real (volume, "test/mounted", ".");

    KAN_TEST_CHECK (kan_virtual_file_system_make_directory (volume, "test/mounted/x/y/z"))
    KAN_TEST_CHECK (kan_virtual_file_system_make_directory (volume, "test/mounted/a/b"))

    KAN_TEST_CHECK (write_text_file (volume, "test/mounted/x/y/file.txt", "Hello, world!"))
    KAN_TEST_CHECK (write_text_file (volume, "test/mounted/x/y/z/file.txt", "Hello, world!"))
    KAN_TEST_CHECK (write_text_file (volume, "test/mounted/a/b/file.txt", "Hello, world!"))

    KAN_TEST_CHECK (kan_virtual_file_system_remove_directory_with_content (volume, "test/mounted/x"))
    KAN_TEST_CHECK (!kan_virtual_file_system_check_existence (volume, "test/mounted/x/y/file.txt"))
    KAN_TEST_CHECK (!kan_virtual_file_system_check_existence (volume, "test/mounted/x/y/z/file.txt"))
    KAN_TEST_CHECK (kan_virtual_file_system_check_existence (volume, "test/mounted/a/b/file.txt"))

    kan_virtual_file_system_volume_unmount (volume, "test/mounted");
    KAN_TEST_CHECK (!kan_virtual_file_system_check_existence (volume, "test/mounted/a/b/file.txt"))
    kan_virtual_file_system_volume_mount_real (volume, "test/mounted", ".");
    KAN_TEST_CHECK (kan_virtual_file_system_check_existence (volume, "test/mounted/a/b/file.txt"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_directory_with_content (volume, "test/mounted/a"))
    KAN_TEST_CHECK (!kan_virtual_file_system_check_existence (volume, "test/mounted/a/b/file.txt"))
    kan_virtual_file_system_volume_destroy (volume);
}

KAN_TEST_CASE (remove_virtual_directory_with_mount_point)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    KAN_TEST_CHECK (kan_virtual_file_system_make_directory (volume, "test"))
    kan_virtual_file_system_volume_mount_real (volume, "test/mounted", ".");
    KAN_TEST_CHECK (write_text_file (volume, "test/mounted/file.txt", "Hello, world!"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_directory_with_content (volume, "test"))

    kan_virtual_file_system_volume_mount_real (volume, "mounted", ".");
    KAN_TEST_CHECK (kan_virtual_file_system_check_existence (volume, "mounted/file.txt"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "mounted/file.txt"))
    KAN_TEST_CHECK (!kan_virtual_file_system_check_existence (volume, "mounted/file.txt"))
    kan_virtual_file_system_volume_destroy (volume);
}

KAN_TEST_CASE (query_status)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    KAN_TEST_CHECK (kan_virtual_file_system_make_directory (volume, "test"))
    kan_virtual_file_system_volume_mount_real (volume, "test/mounted", ".");
    KAN_TEST_CHECK (write_text_file (volume, "test/mounted/file1.txt", "Hello, world!"))
    KAN_TEST_CHECK (write_text_file (volume, "test/mounted/file2.txt", "Hello, world! "))

    struct kan_virtual_file_system_entry_status_t status;
    KAN_TEST_CHECK (kan_virtual_file_system_query_entry (volume, "test/mounted/file1.txt", &status))
    KAN_TEST_CHECK (status.type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
    KAN_TEST_CHECK (status.size == 13u)
    KAN_TEST_CHECK (!status.read_only)
    kan_time_size_t test1_last_modification_time_ns = status.last_modification_time_ns;

    KAN_TEST_CHECK (kan_virtual_file_system_query_entry (volume, "test/mounted/file2.txt", &status))
    KAN_TEST_CHECK (status.type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
    KAN_TEST_CHECK (status.size == 14u)
    KAN_TEST_CHECK (!status.read_only)
    KAN_TEST_CHECK (test1_last_modification_time_ns <= status.last_modification_time_ns)

    KAN_TEST_CHECK (kan_virtual_file_system_query_entry (volume, "test/mounted", &status))
    KAN_TEST_CHECK (status.type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY)

    KAN_TEST_CHECK (kan_virtual_file_system_query_entry (volume, "test", &status))
    KAN_TEST_CHECK (status.type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY)

    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "test/mounted/file1.txt"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "test/mounted/file2.txt"))
    kan_virtual_file_system_volume_destroy (volume);
}

KAN_TEST_CASE (plain_read_only_pack)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    KAN_TEST_CHECK (kan_virtual_file_system_volume_mount_real (volume, "workspace", "."))
    KAN_TEST_CHECK (write_text_file (volume, "workspace/log.txt", "Some text data"))
    KAN_TEST_CHECK (write_text_file (volume, "workspace/.index", "Some index data"))
    KAN_TEST_CHECK (write_text_file (volume, "workspace/no_extension_here", "Hello, world!"))

    kan_virtual_file_system_read_only_pack_builder_t builder = kan_virtual_file_system_read_only_pack_builder_create ();
    struct kan_stream_t *pack_stream = kan_virtual_file_stream_open_for_write (volume, "workspace/data.pack");
    KAN_TEST_ASSERT (kan_virtual_file_system_read_only_pack_builder_begin (builder, pack_stream))

    struct kan_stream_t *file_stream = kan_virtual_file_stream_open_for_read (volume, "workspace/log.txt");
    kan_virtual_file_system_read_only_pack_builder_add (builder, file_stream, "log.txt");
    file_stream->operations->close (file_stream);

    file_stream = kan_virtual_file_stream_open_for_read (volume, "workspace/.index");
    kan_virtual_file_system_read_only_pack_builder_add (builder, file_stream, ".index");
    file_stream->operations->close (file_stream);

    file_stream = kan_virtual_file_stream_open_for_read (volume, "workspace/no_extension_here");
    kan_virtual_file_system_read_only_pack_builder_add (builder, file_stream, "no_extension_here");
    file_stream->operations->close (file_stream);

    KAN_TEST_ASSERT (kan_virtual_file_system_read_only_pack_builder_finalize (builder))
    pack_stream->operations->close (pack_stream);
    kan_virtual_file_system_read_only_pack_builder_destroy (builder);

    KAN_TEST_CHECK (kan_virtual_file_system_volume_mount_read_only_pack (volume, "packed", "data.pack"))
    KAN_TEST_CHECK (read_text_file (volume, "packed/log.txt", "Some text data"))
    KAN_TEST_CHECK (read_text_file (volume, "packed/.index", "Some index data"))
    KAN_TEST_CHECK (read_text_file (volume, "packed/no_extension_here", "Hello, world!"))

    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "workspace/log.txt"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "workspace/.index"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "workspace/no_extension_here"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "workspace/data.pack"))
    kan_virtual_file_system_volume_destroy (volume);
}

KAN_TEST_CASE (hierarchical_read_only_pack)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    KAN_TEST_CHECK (kan_virtual_file_system_volume_mount_real (volume, "workspace", "."))
    KAN_TEST_CHECK (write_text_file (volume, "workspace/log.txt", "Some text data"))
    KAN_TEST_CHECK (write_text_file (volume, "workspace/.index", "Some index data"))
    KAN_TEST_CHECK (write_text_file (volume, "workspace/no_extension_here", "Hello, world!"))

    kan_virtual_file_system_read_only_pack_builder_t builder = kan_virtual_file_system_read_only_pack_builder_create ();
    struct kan_stream_t *pack_stream = kan_virtual_file_stream_open_for_write (volume, "workspace/data.pack");
    KAN_TEST_ASSERT (kan_virtual_file_system_read_only_pack_builder_begin (builder, pack_stream))

    struct kan_stream_t *file_stream = kan_virtual_file_stream_open_for_read (volume, "workspace/log.txt");
    kan_virtual_file_system_read_only_pack_builder_add (builder, file_stream, "sub1/sub2/log.txt");
    file_stream->operations->close (file_stream);

    file_stream = kan_virtual_file_stream_open_for_read (volume, "workspace/.index");
    kan_virtual_file_system_read_only_pack_builder_add (builder, file_stream, "sub1/.index");
    file_stream->operations->close (file_stream);

    file_stream = kan_virtual_file_stream_open_for_read (volume, "workspace/no_extension_here");
    kan_virtual_file_system_read_only_pack_builder_add (builder, file_stream, "sub1/sub2/no_extension_here");
    file_stream->operations->close (file_stream);

    KAN_TEST_ASSERT (kan_virtual_file_system_read_only_pack_builder_finalize (builder))
    pack_stream->operations->close (pack_stream);
    kan_virtual_file_system_read_only_pack_builder_destroy (builder);

    KAN_TEST_CHECK (kan_virtual_file_system_volume_mount_read_only_pack (volume, "packed", "data.pack"))
    KAN_TEST_CHECK (read_text_file (volume, "packed/sub1/sub2/log.txt", "Some text data"))
    KAN_TEST_CHECK (read_text_file (volume, "packed/sub1/.index", "Some index data"))
    KAN_TEST_CHECK (read_text_file (volume, "packed/sub1/sub2/no_extension_here", "Hello, world!"))

    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "workspace/log.txt"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "workspace/.index"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "workspace/no_extension_here"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "workspace/data.pack"))
    kan_virtual_file_system_volume_destroy (volume);
}

KAN_TEST_CASE (directory_iterators)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/some_directory"))
    KAN_TEST_CHECK (kan_virtual_file_system_volume_mount_real (volume, "test/workspace", "."))
    KAN_TEST_CHECK (write_text_file (volume, "test/workspace/log.txt", "Some text data"))
    KAN_TEST_CHECK (write_text_file (volume, "test/workspace/.index", "Some index data"))
    KAN_TEST_CHECK (write_text_file (volume, "test/workspace/no_extension_here", "Hello, world!"))

    kan_virtual_file_system_read_only_pack_builder_t builder = kan_virtual_file_system_read_only_pack_builder_create ();
    struct kan_stream_t *pack_stream = kan_virtual_file_stream_open_for_write (volume, "test/workspace/data.pack");
    KAN_TEST_ASSERT (kan_virtual_file_system_read_only_pack_builder_begin (builder, pack_stream))

    struct kan_stream_t *file_stream = kan_virtual_file_stream_open_for_read (volume, "test/workspace/log.txt");
    kan_virtual_file_system_read_only_pack_builder_add (builder, file_stream, "sub1/sub2/log.txt");
    file_stream->operations->close (file_stream);

    file_stream = kan_virtual_file_stream_open_for_read (volume, "test/workspace/.index");
    kan_virtual_file_system_read_only_pack_builder_add (builder, file_stream, "sub1/.index");
    file_stream->operations->close (file_stream);

    file_stream = kan_virtual_file_stream_open_for_read (volume, "test/workspace/no_extension_here");
    kan_virtual_file_system_read_only_pack_builder_add (builder, file_stream, "sub1/sub2/no_extension_here");
    file_stream->operations->close (file_stream);

    KAN_TEST_ASSERT (kan_virtual_file_system_read_only_pack_builder_finalize (builder))
    pack_stream->operations->close (pack_stream);
    kan_virtual_file_system_read_only_pack_builder_destroy (builder);

    KAN_TEST_CHECK (kan_virtual_file_system_volume_mount_read_only_pack (volume, "test/packed", "data.pack"))

    struct kan_virtual_file_system_directory_iterator_t iterator =
        kan_virtual_file_system_directory_iterator_create (volume, "test");

    bool some_directory_found = false;
    bool workspace_found = false;
    bool packed_found = false;
    const char *entry_name;

    while ((entry_name = kan_virtual_file_system_directory_iterator_advance (&iterator)))
    {
        if ((entry_name[0u] == '.' && entry_name[1u] == '\0') ||
            (entry_name[0u] == '.' && entry_name[1u] == '.' && entry_name[2u] == '\0'))
        {
            continue;
        }

        KAN_TEST_CHECK (!some_directory_found || !workspace_found || !packed_found)
        if (strcmp (entry_name, "some_directory") == 0)
        {
            KAN_TEST_CHECK (!some_directory_found)
            some_directory_found = true;
        }
        else if (strcmp (entry_name, "workspace") == 0)
        {
            KAN_TEST_CHECK (!workspace_found)
            workspace_found = true;
        }
        else if (strcmp (entry_name, "packed") == 0)
        {
            KAN_TEST_CHECK (!packed_found)
            packed_found = true;
        }
        else
        {
            KAN_TEST_CHECK (false)
        }
    }

    KAN_TEST_CHECK (some_directory_found)
    KAN_TEST_CHECK (workspace_found)
    KAN_TEST_CHECK (packed_found)
    kan_virtual_file_system_directory_iterator_destroy (&iterator);

    iterator = kan_virtual_file_system_directory_iterator_create (volume, "test/workspace");
    bool log_txt_found = false;
    bool index_found = false;
    bool no_extension_here = false;
    bool data_pack_found = false;

    while ((entry_name = kan_virtual_file_system_directory_iterator_advance (&iterator)))
    {
        if ((entry_name[0u] == '.' && entry_name[1u] == '\0') ||
            (entry_name[0u] == '.' && entry_name[1u] == '.' && entry_name[2u] == '\0'))
        {
            continue;
        }

        KAN_TEST_CHECK (!log_txt_found || !index_found || !no_extension_here || !data_pack_found)
        if (strcmp (entry_name, "log.txt") == 0)
        {
            KAN_TEST_CHECK (!log_txt_found)
            log_txt_found = true;
        }
        else if (strcmp (entry_name, ".index") == 0)
        {
            KAN_TEST_CHECK (!index_found)
            index_found = true;
        }
        else if (strcmp (entry_name, "no_extension_here") == 0)
        {
            KAN_TEST_CHECK (!no_extension_here)
            no_extension_here = true;
        }
        else if (strcmp (entry_name, "data.pack") == 0)
        {
            KAN_TEST_CHECK (!data_pack_found)
            data_pack_found = true;
        }
        else
        {
            KAN_TEST_CHECK (false)
        }
    }

    KAN_TEST_CHECK (log_txt_found)
    KAN_TEST_CHECK (index_found)
    KAN_TEST_CHECK (no_extension_here)
    KAN_TEST_CHECK (data_pack_found)
    kan_virtual_file_system_directory_iterator_destroy (&iterator);

    iterator = kan_virtual_file_system_directory_iterator_create (volume, "test/packed/sub1");
    bool sub2_found = false;
    bool packed_index_found = false;

    while ((entry_name = kan_virtual_file_system_directory_iterator_advance (&iterator)))
    {
        if ((entry_name[0u] == '.' && entry_name[1u] == '\0') ||
            (entry_name[0u] == '.' && entry_name[1u] == '.' && entry_name[2u] == '\0'))
        {
            continue;
        }

        KAN_TEST_CHECK (!sub2_found || !packed_index_found)
        if (strcmp (entry_name, "sub2") == 0)
        {
            KAN_TEST_CHECK (!sub2_found)
            sub2_found = true;
        }
        else if (strcmp (entry_name, ".index") == 0)
        {
            KAN_TEST_CHECK (!packed_index_found)
            packed_index_found = true;
        }
        else
        {
            KAN_TEST_CHECK (false)
        }
    }

    KAN_TEST_CHECK (sub2_found)
    KAN_TEST_CHECK (packed_index_found)
    kan_virtual_file_system_directory_iterator_destroy (&iterator);

    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "test/workspace/log.txt"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "test/workspace/.index"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "test/workspace/no_extension_here"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "test/workspace/data.pack"))
    kan_virtual_file_system_volume_destroy (volume);
}

KAN_TEST_CASE (watcher_add_file)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/watched/something"))
    KAN_TEST_ASSERT (kan_virtual_file_system_volume_mount_real (volume, "test/watched/something/mounted", "."))
    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/watched/something/mounted/a"))
    KAN_TEST_ASSERT (write_text_file (volume, "test/watched/something/mounted/a/test1.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file (volume, "test/watched/something/mounted/a/test2.txt", "Hello, world!"))

    kan_virtual_file_system_watcher_t watcher = kan_virtual_file_system_watcher_create (volume, "test/watched");
    kan_virtual_file_system_watcher_iterator_t iterator = kan_virtual_file_system_watcher_iterator_create (watcher);
    give_some_time_for_poll ();

    KAN_TEST_ASSERT (write_text_file (volume, "test/watched/something/mounted/a/test3.txt", "New file"))
    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/unwatched"))
    give_some_time_for_poll ();

    const struct kan_virtual_file_system_watcher_event_t *event =
        kan_virtual_file_system_watcher_iterator_get (watcher, iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->event_type == KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_ADDED)
    KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
    KAN_TEST_CHECK (strcmp (event->path_container.path, "test/watched/something/mounted/a/test3.txt") == 0)

    // File addition might be reported as creation + modification, because file is created by stream open operation and
    // data is written by stream write operation and watcher might encounter state between these two operations.
    iterator = kan_virtual_file_system_watcher_iterator_advance (watcher, iterator);

    while ((event = kan_virtual_file_system_watcher_iterator_get (watcher, iterator)))
    {
        KAN_TEST_CHECK (event->event_type == KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_MODIFIED)
        KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        KAN_TEST_CHECK (strcmp (event->path_container.path, "test/watched/something/mounted/a/test3.txt") == 0)
        iterator = kan_virtual_file_system_watcher_iterator_advance (watcher, iterator);
    }

    kan_virtual_file_system_watcher_iterator_destroy (watcher, iterator);
    kan_virtual_file_system_watcher_destroy (watcher);
    KAN_TEST_CHECK (kan_virtual_file_system_remove_directory_with_content (volume, "test/watched/something/mounted/a"))
    kan_virtual_file_system_volume_destroy (volume);
}

KAN_TEST_CASE (watcher_modify_file)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/watched/something"))
    KAN_TEST_ASSERT (kan_virtual_file_system_volume_mount_real (volume, "test/watched/something/mounted", "."))
    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/watched/something/mounted/a"))
    KAN_TEST_ASSERT (write_text_file (volume, "test/watched/something/mounted/a/test1.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file (volume, "test/watched/something/mounted/a/test2.txt", "Hello, world!"))

    kan_virtual_file_system_watcher_t watcher = kan_virtual_file_system_watcher_create (volume, "test/watched");
    kan_virtual_file_system_watcher_iterator_t iterator = kan_virtual_file_system_watcher_iterator_create (watcher);
    give_some_time_for_poll ();

    KAN_TEST_ASSERT (write_text_file (volume, "test/watched/something/mounted/a/test2.txt", "New content"))
    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/unwatched"))
    give_some_time_for_poll ();

    const struct kan_virtual_file_system_watcher_event_t *event =
        kan_virtual_file_system_watcher_iterator_get (watcher, iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->event_type == KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_MODIFIED)
    KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
    KAN_TEST_CHECK (strcmp (event->path_container.path, "test/watched/something/mounted/a/test2.txt") == 0)

    // File write might be processed as several operations in some cases and watcher might observe state between two
    // writes. Therefore, we need to be able to correctly process several modification events.
    iterator = kan_virtual_file_system_watcher_iterator_advance (watcher, iterator);

    while ((event = kan_virtual_file_system_watcher_iterator_get (watcher, iterator)))
    {
        KAN_TEST_CHECK (event->event_type == KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_MODIFIED)
        KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        KAN_TEST_CHECK (strcmp (event->path_container.path, "test/watched/something/mounted/a/test2.txt") == 0)
        iterator = kan_virtual_file_system_watcher_iterator_advance (watcher, iterator);
    }

    kan_virtual_file_system_watcher_iterator_destroy (watcher, iterator);
    kan_virtual_file_system_watcher_destroy (watcher);
    KAN_TEST_CHECK (kan_virtual_file_system_remove_directory_with_content (volume, "test/watched/something/mounted/a"))
    kan_virtual_file_system_volume_destroy (volume);
}

KAN_TEST_CASE (watcher_delete_file)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/watched/something"))
    KAN_TEST_ASSERT (kan_virtual_file_system_volume_mount_real (volume, "test/watched/something/mounted", "."))
    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/watched/something/mounted/a"))
    KAN_TEST_ASSERT (write_text_file (volume, "test/watched/something/mounted/a/test1.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file (volume, "test/watched/something/mounted/a/test2.txt", "Hello, world!"))

    kan_virtual_file_system_watcher_t watcher = kan_virtual_file_system_watcher_create (volume, "test/watched");
    kan_virtual_file_system_watcher_iterator_t iterator = kan_virtual_file_system_watcher_iterator_create (watcher);
    give_some_time_for_poll ();

    KAN_TEST_ASSERT (kan_virtual_file_system_remove_file (volume, "test/watched/something/mounted/a/test1.txt"))
    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/unwatched"))
    give_some_time_for_poll ();

    const struct kan_virtual_file_system_watcher_event_t *event =
        kan_virtual_file_system_watcher_iterator_get (watcher, iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->event_type == KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_REMOVED)
    KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
    KAN_TEST_CHECK (strcmp (event->path_container.path, "test/watched/something/mounted/a/test1.txt") == 0)

    iterator = kan_virtual_file_system_watcher_iterator_advance (watcher, iterator);
    KAN_TEST_CHECK (!kan_virtual_file_system_watcher_iterator_get (watcher, iterator))

    kan_virtual_file_system_watcher_iterator_destroy (watcher, iterator);
    kan_virtual_file_system_watcher_destroy (watcher);
    KAN_TEST_CHECK (kan_virtual_file_system_remove_directory_with_content (volume, "test/watched/something/mounted/a"))
    kan_virtual_file_system_volume_destroy (volume);
}

KAN_TEST_CASE (watcher_add_virtual_directory)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/watched/something"))
    KAN_TEST_ASSERT (kan_virtual_file_system_volume_mount_real (volume, "test/watched/something/mounted", "."))
    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/watched/something/mounted/a"))
    KAN_TEST_ASSERT (write_text_file (volume, "test/watched/something/mounted/a/test1.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file (volume, "test/watched/something/mounted/a/test2.txt", "Hello, world!"))

    kan_virtual_file_system_watcher_t watcher = kan_virtual_file_system_watcher_create (volume, "test/watched");
    kan_virtual_file_system_watcher_iterator_t iterator = kan_virtual_file_system_watcher_iterator_create (watcher);
    give_some_time_for_poll ();

    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/watched/something/new_directory"))
    give_some_time_for_poll ();

    const struct kan_virtual_file_system_watcher_event_t *event =
        kan_virtual_file_system_watcher_iterator_get (watcher, iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->event_type == KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_ADDED)
    KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY)
    KAN_TEST_CHECK (strcmp (event->path_container.path, "test/watched/something/new_directory") == 0)

    iterator = kan_virtual_file_system_watcher_iterator_advance (watcher, iterator);
    KAN_TEST_CHECK (!kan_virtual_file_system_watcher_iterator_get (watcher, iterator))

    kan_virtual_file_system_watcher_iterator_destroy (watcher, iterator);
    kan_virtual_file_system_watcher_destroy (watcher);
    KAN_TEST_CHECK (kan_virtual_file_system_remove_directory_with_content (volume, "test/watched/something/mounted/a"))
    kan_virtual_file_system_volume_destroy (volume);
}

KAN_TEST_CASE (watcher_add_real_directory)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/watched/something"))
    KAN_TEST_ASSERT (kan_virtual_file_system_volume_mount_real (volume, "test/watched/something/mounted", "."))
    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/watched/something/mounted/a"))
    KAN_TEST_ASSERT (write_text_file (volume, "test/watched/something/mounted/a/test1.txt", "Hello, world!"))
    KAN_TEST_ASSERT (write_text_file (volume, "test/watched/something/mounted/a/test2.txt", "Hello, world!"))

    kan_virtual_file_system_watcher_t watcher = kan_virtual_file_system_watcher_create (volume, "test/watched");
    kan_virtual_file_system_watcher_iterator_t iterator = kan_virtual_file_system_watcher_iterator_create (watcher);
    give_some_time_for_poll ();

    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/watched/something/mounted/a/new_directory"))
    give_some_time_for_poll ();

    const struct kan_virtual_file_system_watcher_event_t *event =
        kan_virtual_file_system_watcher_iterator_get (watcher, iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->event_type == KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_ADDED)
    KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY)
    KAN_TEST_CHECK (strcmp (event->path_container.path, "test/watched/something/mounted/a/new_directory") == 0)

    iterator = kan_virtual_file_system_watcher_iterator_advance (watcher, iterator);
    KAN_TEST_CHECK (!kan_virtual_file_system_watcher_iterator_get (watcher, iterator))

    kan_virtual_file_system_watcher_iterator_destroy (watcher, iterator);
    kan_virtual_file_system_watcher_destroy (watcher);
    KAN_TEST_CHECK (kan_virtual_file_system_remove_directory_with_content (volume, "test/watched/something/mounted/a"))
    kan_virtual_file_system_volume_destroy (volume);
}

KAN_TEST_CASE (wathcer_unmount_and_mount)
{
    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    KAN_TEST_ASSERT (kan_virtual_file_system_make_directory (volume, "test/some_directory"))
    KAN_TEST_CHECK (kan_virtual_file_system_volume_mount_real (volume, "test/workspace", "."))
    KAN_TEST_CHECK (write_text_file (volume, "test/workspace/log.txt", "Some text data"))
    KAN_TEST_CHECK (write_text_file (volume, "test/workspace/.index", "Some index data"))
    KAN_TEST_CHECK (write_text_file (volume, "test/workspace/no_extension_here", "Hello, world!"))

    kan_virtual_file_system_read_only_pack_builder_t builder = kan_virtual_file_system_read_only_pack_builder_create ();
    struct kan_stream_t *pack_stream = kan_virtual_file_stream_open_for_write (volume, "test/workspace/data.pack");
    KAN_TEST_ASSERT (kan_virtual_file_system_read_only_pack_builder_begin (builder, pack_stream))

    struct kan_stream_t *file_stream = kan_virtual_file_stream_open_for_read (volume, "test/workspace/log.txt");
    kan_virtual_file_system_read_only_pack_builder_add (builder, file_stream, "sub1/sub2/log.txt");
    file_stream->operations->close (file_stream);

    file_stream = kan_virtual_file_stream_open_for_read (volume, "test/workspace/.index");
    kan_virtual_file_system_read_only_pack_builder_add (builder, file_stream, "sub1/.index");
    file_stream->operations->close (file_stream);

    file_stream = kan_virtual_file_stream_open_for_read (volume, "test/workspace/no_extension_here");
    kan_virtual_file_system_read_only_pack_builder_add (builder, file_stream, "sub1/sub2/no_extension_here");
    file_stream->operations->close (file_stream);

    KAN_TEST_ASSERT (kan_virtual_file_system_read_only_pack_builder_finalize (builder))
    pack_stream->operations->close (pack_stream);
    kan_virtual_file_system_read_only_pack_builder_destroy (builder);

    KAN_TEST_CHECK (kan_virtual_file_system_volume_mount_read_only_pack (volume, "test/packed", "data.pack"))
    kan_virtual_file_system_watcher_t watcher = kan_virtual_file_system_watcher_create (volume, "test");
    kan_virtual_file_system_watcher_iterator_t iterator = kan_virtual_file_system_watcher_iterator_create (watcher);
    give_some_time_for_poll ();

    kan_virtual_file_system_volume_unmount (volume, "test/workspace");
    kan_virtual_file_system_volume_unmount (volume, "test/packed");

    bool workspace_log_txt_removed = false;
    bool workspace_index_removed = false;
    bool workspace_no_extension_here_removed = false;
    bool workspace_data_pack_removed = false;
    bool workspace_removed = false;
    bool packed_sub1_sub2_log_txt_removed = false;
    bool packed_sub1_sub2_no_extension_here_removed = false;
    bool packed_sub1_sub2_removed = false;
    bool packed_sub1_index_removed = false;
    bool packed_sub1_removed = false;
    bool packed_removed = false;

    const struct kan_virtual_file_system_watcher_event_t *event;
    while ((event = kan_virtual_file_system_watcher_iterator_get (watcher, iterator)))
    {
        KAN_TEST_CHECK (!workspace_log_txt_removed || !workspace_index_removed ||
                        !workspace_no_extension_here_removed || !workspace_data_pack_removed || !workspace_removed ||
                        !packed_sub1_sub2_log_txt_removed || !packed_sub1_sub2_no_extension_here_removed ||
                        !packed_sub1_sub2_removed || !packed_sub1_index_removed || !packed_sub1_removed ||
                        !packed_removed)

        KAN_TEST_CHECK (event->event_type == KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_REMOVED)
        if (strcmp (event->path_container.path, "test/workspace/log.txt") == 0)
        {
            KAN_TEST_CHECK (!workspace_log_txt_removed)
            workspace_log_txt_removed = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        }
        else if (strcmp (event->path_container.path, "test/workspace/.index") == 0)
        {
            KAN_TEST_CHECK (!workspace_index_removed)
            workspace_index_removed = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        }
        else if (strcmp (event->path_container.path, "test/workspace/no_extension_here") == 0)
        {
            KAN_TEST_CHECK (!workspace_no_extension_here_removed)
            workspace_no_extension_here_removed = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        }
        else if (strcmp (event->path_container.path, "test/workspace/data.pack") == 0)
        {
            KAN_TEST_CHECK (!workspace_data_pack_removed)
            workspace_data_pack_removed = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        }
        else if (strcmp (event->path_container.path, "test/workspace") == 0)
        {
            KAN_TEST_CHECK (!workspace_removed)
            workspace_removed = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY)
        }
        else if (strcmp (event->path_container.path, "test/packed/sub1/sub2/log.txt") == 0)
        {
            KAN_TEST_CHECK (!packed_sub1_sub2_log_txt_removed)
            packed_sub1_sub2_log_txt_removed = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        }
        else if (strcmp (event->path_container.path, "test/packed/sub1/sub2/no_extension_here") == 0)
        {
            KAN_TEST_CHECK (!packed_sub1_sub2_no_extension_here_removed)
            packed_sub1_sub2_no_extension_here_removed = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        }
        else if (strcmp (event->path_container.path, "test/packed/sub1/sub2") == 0)
        {
            KAN_TEST_CHECK (!packed_sub1_sub2_removed)
            packed_sub1_sub2_removed = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY)
        }
        else if (strcmp (event->path_container.path, "test/packed/sub1/.index") == 0)
        {
            KAN_TEST_CHECK (!packed_sub1_index_removed)
            packed_sub1_index_removed = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        }
        else if (strcmp (event->path_container.path, "test/packed/sub1") == 0)
        {
            KAN_TEST_CHECK (!packed_sub1_removed)
            packed_sub1_removed = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY)
        }
        else if (strcmp (event->path_container.path, "test/packed") == 0)
        {
            KAN_TEST_CHECK (!packed_removed)
            packed_removed = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY)
        }
        else
        {
            KAN_TEST_CHECK (false)
        }

        iterator = kan_virtual_file_system_watcher_iterator_advance (watcher, iterator);
    }

    KAN_TEST_CHECK (workspace_log_txt_removed)
    KAN_TEST_CHECK (workspace_index_removed)
    KAN_TEST_CHECK (workspace_no_extension_here_removed)
    KAN_TEST_CHECK (workspace_data_pack_removed)
    KAN_TEST_CHECK (workspace_removed)
    KAN_TEST_CHECK (packed_sub1_sub2_log_txt_removed)
    KAN_TEST_CHECK (packed_sub1_sub2_no_extension_here_removed)
    KAN_TEST_CHECK (packed_sub1_sub2_removed)
    KAN_TEST_CHECK (packed_sub1_index_removed)
    KAN_TEST_CHECK (packed_sub1_removed)
    KAN_TEST_CHECK (packed_removed)

    KAN_TEST_CHECK (kan_virtual_file_system_volume_mount_real (volume, "test/workspace", "."))
    KAN_TEST_CHECK (kan_virtual_file_system_volume_mount_read_only_pack (volume, "test/packed", "data.pack"))

    bool workspace_log_txt_added = false;
    bool workspace_index_added = false;
    bool workspace_no_extension_here_added = false;
    bool workspace_data_pack_added = false;
    bool workspace_added = false;
    bool packed_sub1_sub2_log_txt_added = false;
    bool packed_sub1_sub2_no_extension_here_added = false;
    bool packed_sub1_sub2_added = false;
    bool packed_sub1_index_added = false;
    bool packed_sub1_added = false;
    bool packed_added = false;

    while ((event = kan_virtual_file_system_watcher_iterator_get (watcher, iterator)))
    {
        KAN_TEST_CHECK (!workspace_log_txt_added || !workspace_index_added || !workspace_no_extension_here_added ||
                        !workspace_data_pack_added || !workspace_added || !packed_sub1_sub2_log_txt_added ||
                        !packed_sub1_sub2_no_extension_here_added || !packed_sub1_sub2_added ||
                        !packed_sub1_index_added || !packed_sub1_added || !packed_added)

        KAN_TEST_CHECK (event->event_type == KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_ADDED)
        if (strcmp (event->path_container.path, "test/workspace/log.txt") == 0)
        {
            KAN_TEST_CHECK (!workspace_log_txt_added)
            workspace_log_txt_added = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        }
        else if (strcmp (event->path_container.path, "test/workspace/.index") == 0)
        {
            KAN_TEST_CHECK (!workspace_index_added)
            workspace_index_added = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        }
        else if (strcmp (event->path_container.path, "test/workspace/no_extension_here") == 0)
        {
            KAN_TEST_CHECK (!workspace_no_extension_here_added)
            workspace_no_extension_here_added = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        }
        else if (strcmp (event->path_container.path, "test/workspace/data.pack") == 0)
        {
            KAN_TEST_CHECK (!workspace_data_pack_added)
            workspace_data_pack_added = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        }
        else if (strcmp (event->path_container.path, "test/workspace") == 0)
        {
            KAN_TEST_CHECK (!workspace_added)
            workspace_added = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY)
        }
        else if (strcmp (event->path_container.path, "test/packed/sub1/sub2/log.txt") == 0)
        {
            KAN_TEST_CHECK (!packed_sub1_sub2_log_txt_added)
            packed_sub1_sub2_log_txt_added = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        }
        else if (strcmp (event->path_container.path, "test/packed/sub1/sub2/no_extension_here") == 0)
        {
            KAN_TEST_CHECK (!packed_sub1_sub2_no_extension_here_added)
            packed_sub1_sub2_no_extension_here_added = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        }
        else if (strcmp (event->path_container.path, "test/packed/sub1/sub2") == 0)
        {
            KAN_TEST_CHECK (!packed_sub1_sub2_added)
            packed_sub1_sub2_added = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY)
        }
        else if (strcmp (event->path_container.path, "test/packed/sub1/.index") == 0)
        {
            KAN_TEST_CHECK (!packed_sub1_index_added)
            packed_sub1_index_added = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        }
        else if (strcmp (event->path_container.path, "test/packed/sub1") == 0)
        {
            KAN_TEST_CHECK (!packed_sub1_added)
            packed_sub1_added = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY)
        }
        else if (strcmp (event->path_container.path, "test/packed") == 0)
        {
            KAN_TEST_CHECK (!packed_added)
            packed_added = true;
            KAN_TEST_CHECK (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY)
        }
        else
        {
            KAN_TEST_CHECK (false)
        }

        iterator = kan_virtual_file_system_watcher_iterator_advance (watcher, iterator);
    }

    KAN_TEST_CHECK (workspace_log_txt_added)
    KAN_TEST_CHECK (workspace_index_added)
    KAN_TEST_CHECK (workspace_no_extension_here_added)
    KAN_TEST_CHECK (workspace_data_pack_added)
    KAN_TEST_CHECK (workspace_added)
    KAN_TEST_CHECK (packed_sub1_sub2_log_txt_added)
    KAN_TEST_CHECK (packed_sub1_sub2_no_extension_here_added)
    KAN_TEST_CHECK (packed_sub1_sub2_added)
    KAN_TEST_CHECK (packed_sub1_index_added)
    KAN_TEST_CHECK (packed_sub1_added)
    KAN_TEST_CHECK (packed_added)

    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "test/workspace/log.txt"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "test/workspace/.index"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "test/workspace/no_extension_here"))
    KAN_TEST_CHECK (kan_virtual_file_system_remove_file (volume, "test/workspace/data.pack"))

    kan_virtual_file_system_watcher_iterator_destroy (watcher, iterator);
    kan_virtual_file_system_watcher_destroy (watcher);
    kan_virtual_file_system_volume_destroy (volume);
}
