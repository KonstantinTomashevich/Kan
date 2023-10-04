register_concrete (test_log)
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT log CONCRETE_INTERFACE testing)

abstract_get_implementations (ABSTRACT log OUTPUT MEMORY_PROFILER_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${MEMORY_PROFILER_IMPLEMENTATIONS})
    set (SHARED_NAME "test_log_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT error=sdl hash=djb2 log=${IMPLEMENTATION} memory=kan memory_profiler=none threading=sdl
            CONCRETE container testing test_log)

    shared_library_verify ()
    shared_library_copy_linked_artefacts ()
    kan_setup_tests (TEST_UNIT test_log TEST_SHARED_LIBRARY "${SHARED_NAME}")
endforeach ()
