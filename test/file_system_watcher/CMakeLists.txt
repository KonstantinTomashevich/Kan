register_concrete (test_file_system_watcher)
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT file_system file_system_watcher platform CONCRETE_INTERFACE testing)

abstract_get_implementations (ABSTRACT file_system_watcher OUTPUT FILE_SYSTEM_WATCHER_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${FILE_SYSTEM_WATCHER_IMPLEMENTATIONS})
    set (SHARED_NAME "test_file_system_watcher_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT cpu_dispatch=kan cpu_profiler=none error=sdl file_system=platform_default
            file_system_watcher=${IMPLEMENTATION} hash=djb2 log=kan memory=kan memory_profiler=none platform=sdl
            stream=kan threading=sdl
            CONCRETE container testing test_file_system_watcher)

    shared_library_verify ()
    shared_library_copy_linked_artefacts ()
    kan_setup_tests (TEST_UNIT test_file_system_watcher TEST_SHARED_LIBRARY "${SHARED_NAME}")
endforeach ()
