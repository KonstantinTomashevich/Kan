register_concrete (test_file_system)
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT file_system CONCRETE_INTERFACE testing)

abstract_get_implementations (ABSTRACT file_system OUTPUT FILE_SYSTEM_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${FILE_SYSTEM_IMPLEMENTATIONS})
    set (SHARED_NAME "test_file_system_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT error=sdl file_system=${IMPLEMENTATION} hash=djb2 log=kan memory=kan memory_profiler=none
            stream=kan threading=sdl
            CONCRETE container testing test_file_system)

    shared_library_verify ()
    shared_library_copy_linked_artefacts ()
    kan_setup_tests (TEST_UNIT test_file_system TEST_SHARED_LIBRARY "${SHARED_NAME}")
endforeach ()
