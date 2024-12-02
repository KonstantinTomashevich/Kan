register_concrete (test_file_system_watcher)
concrete_sources ("*.c")
concrete_require (
        SCOPE PUBLIC
        ABSTRACT file_system file_system_watcher platform precise_time
        CONCRETE_INTERFACE testing)

# Unfortunately, running all tests at once makes NTFS behavior a little bit less predictable making tests fail.
# It is kind of difficult to modify tests to take everything that NTFS might throw at us into account.
# Therefore, we just run these tests in serial mode on Windows.
if (WIN32)
    set (RUN_SERIAL ON)
else ()
    set (RUN_SERIAL OFF)
endif ()

abstract_get_implementations (ABSTRACT file_system_watcher OUTPUT FILE_SYSTEM_WATCHER_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${FILE_SYSTEM_WATCHER_IMPLEMENTATIONS})
    set (SHARED_NAME "test_file_system_watcher_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT
            cpu_dispatch=kan cpu_profiler=default error=sdl file_system=platform_default
            file_system_watcher=${IMPLEMENTATION} hash=djb2 log=kan memory=kan memory_profiler=default platform=sdl
            precise_time=sdl reflection=kan stream=kan threading=sdl
            CONCRETE container testing test_file_system_watcher)

    shared_library_verify ()
    shared_library_copy_linked_artefacts ()
    kan_setup_tests (TEST_UNIT test_file_system_watcher TEST_SHARED_LIBRARY "${SHARED_NAME}"
            PROPERTIES RUN_SERIAL ${RUN_SERIAL})
endforeach ()
