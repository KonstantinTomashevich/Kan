register_concrete (test_memory_profiler)
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT memory_profiler CONCRETE_INTERFACE testing)
setup_core_preprocessing ()

abstract_get_implementations (ABSTRACT memory_profiler OUTPUT MEMORY_PROFILER_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${MEMORY_PROFILER_IMPLEMENTATIONS})
    if (IMPLEMENTATION STREQUAL "none")
        continue ()
    endif ()

    set (SHARED_NAME "test_memory_profiler_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT error=sdl hash=djb2 log=kan memory=kan memory_profiler=${IMPLEMENTATION} threading=sdl
            CONCRETE container testing test_memory_profiler)

    shared_library_verify ()
    shared_library_copy_linked_artefacts ()
    kan_setup_tests (TEST_UNIT test_memory_profiler TEST_SHARED_LIBRARY "${SHARED_NAME}")
endforeach ()
