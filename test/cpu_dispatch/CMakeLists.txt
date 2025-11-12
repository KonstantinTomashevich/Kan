register_concrete (test_cpu_dispatch)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT cpu_dispatch precise_time CONCRETE_INTERFACE testing)
setup_core_preprocessing ()

abstract_get_implementations (ABSTRACT cpu_dispatch OUTPUT MEMORY_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${MEMORY_IMPLEMENTATIONS})
    set (SHARED_NAME "test_cpu_dispatch_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT
            cpu_dispatch=${IMPLEMENTATION} cpu_profiler=default error=sdl hash=djb2 log=kan memory=kan
            memory_profiler=default reflection=kan
            platform=sdl precise_time=sdl threading=sdl

            CONCRETE container testing test_cpu_dispatch)

    shared_library_verify ()
    shared_library_copy_linked_artefacts ()

    # We run cpu dispatch tests in serial mode, because they use all the cores
    # and make everything really slow if they are being run in parallel.
    kan_setup_tests (
            TEST_UNIT test_cpu_dispatch TEST_SHARED_LIBRARY "${SHARED_NAME}"
            PROPERTIES RUN_SERIAL ON TIMEOUT 2)
endforeach ()
