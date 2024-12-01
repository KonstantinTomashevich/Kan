register_concrete (test_virtual_file_system)
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT platform precise_time virtual_file_system CONCRETE_INTERFACE testing)

# Unfortunately, running all tests at once makes NTFS behavior a little bit less predictable making tests fail.
# It is kind of difficult to modify tests to take everything that NTFS might throw at us into account.
# Therefore, we just run these tests in serial mode on Windows.
if (WIN32)
    set (RUN_SERIAL ON)
else ()
    set (RUN_SERIAL OFF)
endif ()

abstract_get_implementations (ABSTRACT virtual_file_system OUTPUT VIRTUAL_FILE_SYSTEM_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${VIRTUAL_FILE_SYSTEM_IMPLEMENTATIONS})
    set (SHARED_NAME "test_virtual_file_system_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT cpu_dispatch=kan cpu_profiler=default error=sdl file_system=platform_default
            file_system_watcher=user_level hash=djb2 log=kan memory=kan memory_profiler=default platform=sdl
            precise_time=sdl reflection=kan stream=kan threading=sdl virtual_file_system=${IMPLEMENTATION}
            CONCRETE container readable_data serialization testing test_virtual_file_system)

    shared_library_verify ()
    shared_library_copy_linked_artefacts ()
    kan_setup_tests (TEST_UNIT test_virtual_file_system TEST_SHARED_LIBRARY "${SHARED_NAME}"
            PROPERTIES RUN_SERIAL ${RUN_SERIAL})
endforeach ()
