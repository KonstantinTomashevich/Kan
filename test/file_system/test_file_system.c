#include <string.h>

#include <kan/file_system/entry.h>
#include <kan/file_system/stream.h>
#include <kan/testing/testing.h>

static kan_bool_t write_text_file (const char *file, const char *content)
{
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (file, KAN_TRUE);
    if (!stream || !kan_stream_is_writeable (stream))
    {
        return KAN_FALSE;
    }

    const kan_instance_size_t content_length = (kan_instance_size_t) strlen (content);
    const kan_bool_t result = stream->operations->write (stream, strlen (content), content) == content_length;

    stream->operations->close (stream);
    KAN_TEST_CHECK (kan_file_system_check_existence (file))
    return result;
}

KAN_TEST_CASE (create_and_remove_empty_directory)
{
    KAN_TEST_CHECK (kan_file_system_make_directory ("test_directory"));
    KAN_TEST_CHECK (kan_file_system_remove_empty_directory ("test_directory"))
    KAN_TEST_CHECK (!kan_file_system_check_existence ("test_directory"))
}

KAN_TEST_CASE (create_and_remove_two_level_directories)
{
    KAN_TEST_CHECK (kan_file_system_make_directory ("test1"));
    KAN_TEST_CHECK (kan_file_system_make_directory ("test1/test2"))
    KAN_TEST_CHECK (kan_file_system_remove_empty_directory ("test1/test2"))
    KAN_TEST_CHECK (kan_file_system_remove_empty_directory ("test1"));
    KAN_TEST_CHECK (!kan_file_system_check_existence ("test1/test2"))
    KAN_TEST_CHECK (!kan_file_system_check_existence ("test1"))
}

KAN_TEST_CASE (create_and_remove_file)
{
    KAN_TEST_CHECK (write_text_file ("test.txt", "Hello, world!"));
    KAN_TEST_CHECK (kan_file_system_remove_file ("test.txt"))
    KAN_TEST_CHECK (!kan_file_system_check_existence ("test.txt"))
}

KAN_TEST_CASE (create_and_remove_directory_with_files)
{
    KAN_TEST_CHECK (kan_file_system_make_directory ("test_directory"));
    KAN_TEST_CHECK (write_text_file ("test_directory/test1.txt", "Hello, world!"))
    KAN_TEST_CHECK (write_text_file ("test_directory/test2.txt", "Hello, world!"))
    KAN_TEST_CHECK (kan_file_system_remove_directory_with_content ("test_directory"))
    KAN_TEST_CHECK (!kan_file_system_check_existence ("test_directory/test1.txt"))
    KAN_TEST_CHECK (!kan_file_system_check_existence ("test_directory/test2.txt"))
    KAN_TEST_CHECK (!kan_file_system_check_existence ("test_directory"))
}

KAN_TEST_CASE (create_and_remove_hierarchy_with_files)
{
    KAN_TEST_CHECK (kan_file_system_make_directory ("test_directory"));
    KAN_TEST_CHECK (write_text_file ("test_directory/test1.txt", "Hello, world!"))
    KAN_TEST_CHECK (write_text_file ("test_directory/test2.txt", "Hello, world!"))
    KAN_TEST_CHECK (kan_file_system_make_directory ("test_directory/subdirectory"));
    KAN_TEST_CHECK (kan_file_system_make_directory ("test_directory/subdirectory/another"));
    KAN_TEST_CHECK (write_text_file ("test_directory/subdirectory/another/log.log", "Hello, world!"))
    KAN_TEST_CHECK (kan_file_system_remove_directory_with_content ("test_directory"))

    KAN_TEST_CHECK (!kan_file_system_check_existence ("test_directory/subdirectory/another/log.log"))
    KAN_TEST_CHECK (!kan_file_system_check_existence ("test_directory/subdirectory/another"))
    KAN_TEST_CHECK (!kan_file_system_check_existence ("test_directory/subdirectory"))
    KAN_TEST_CHECK (!kan_file_system_check_existence ("test_directory/test1.txt"))
    KAN_TEST_CHECK (!kan_file_system_check_existence ("test_directory/test2.txt"))
    KAN_TEST_CHECK (!kan_file_system_check_existence ("test_directory"))
}

KAN_TEST_CASE (query_status)
{
    KAN_TEST_CHECK (kan_file_system_make_directory ("test_directory"));
    KAN_TEST_CHECK (write_text_file ("test_directory/test1.txt", "Hello, world!"))
    KAN_TEST_CHECK (write_text_file ("test_directory/test2.txt", "Hello, world!!"))

    struct kan_file_system_entry_status_t status;
    KAN_TEST_CHECK (kan_file_system_query_entry ("test_directory/test1.txt", &status))
    KAN_TEST_CHECK (status.type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE)
    KAN_TEST_CHECK (status.size == 13u)
    KAN_TEST_CHECK (!status.read_only)
    kan_time_size_t test1_last_modification_time_ns = status.last_modification_time_ns;

    KAN_TEST_CHECK (kan_file_system_query_entry ("test_directory/test2.txt", &status))
    KAN_TEST_CHECK (status.type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE)
    KAN_TEST_CHECK (status.size == 14u)
    KAN_TEST_CHECK (!status.read_only)
    KAN_TEST_CHECK (test1_last_modification_time_ns <= status.last_modification_time_ns)

    KAN_TEST_CHECK (kan_file_system_query_entry ("test_directory", &status))
    KAN_TEST_CHECK (status.type == KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY)

    KAN_TEST_CHECK (kan_file_system_remove_directory_with_content ("test_directory"))
}
