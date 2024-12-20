register_concrete (test_repository)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT cpu_dispatch repository CONCRETE_INTERFACE testing)

concrete_preprocessing_queue_step_preprocess ()
reflection_preprocessor_setup_step (GLOB "*.c")
register_unit_reflection ()

abstract_get_implementations (ABSTRACT repository OUTPUT REPOSITORY_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${REPOSITORY_IMPLEMENTATIONS})
    set (SHARED_NAME "test_repository_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT
            cpu_dispatch=kan cpu_profiler=default error=sdl hash=djb2 log=kan memory=kan memory_profiler=default
            platform=sdl precise_time=sdl reflection=kan repository=${IMPLEMENTATION} threading=sdl
            CONCRETE container testing test_repository)

    shared_library_verify ()
    shared_library_copy_linked_artefacts ()
    kan_setup_tests (TEST_UNIT test_repository TEST_SHARED_LIBRARY "${SHARED_NAME}")
endforeach ()
