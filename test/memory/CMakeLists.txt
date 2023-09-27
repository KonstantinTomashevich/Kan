register_concrete (test_memory)
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT memory CONCRETE_INTERFACE testing)

abstract_get_implementations (ABSTRACT memory OUTPUT MEMORY_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${MEMORY_IMPLEMENTATIONS})
    set (SHARED_NAME "test_memory_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT error=sdl memory=${IMPLEMENTATION} memory_profiler=none threading=sdl
            CONCRETE testing test_memory)

    shared_library_verify ()
    shared_library_copy_linked_artefacts ()
    kan_setup_tests (TEST_UNIT test_memory TEST_SHARED_LIBRARY "${SHARED_NAME}")
endforeach ()
