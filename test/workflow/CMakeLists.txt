register_concrete (test_workflow)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT cpu_dispatch platform precise_time workflow CONCRETE_INTERFACE testing)
setup_core_preprocessing ()

abstract_get_implementations (ABSTRACT workflow OUTPUT WORKFLOW_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${WORKFLOW_IMPLEMENTATIONS})
    set (SHARED_NAME "test_workflow_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT
            cpu_dispatch=kan cpu_profiler=default error=sdl hash=djb2 log=kan memory=kan memory_profiler=default
            platform=sdl precise_time=sdl reflection=kan workflow=${IMPLEMENTATION} threading=sdl
            CONCRETE container testing test_workflow)

    shared_library_verify ()
    shared_library_copy_linked_artefacts ()
    kan_setup_tests (TEST_UNIT test_workflow TEST_SHARED_LIBRARY "${SHARED_NAME}")
endforeach ()
