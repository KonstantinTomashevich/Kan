register_concrete (test_repository)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

c_interface_scanner_setup (GLOB "*.h")
reflection_generator_setup (GLOB "*.h")

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")
concrete_require (SCOPE PUBLIC ABSTRACT cpu_dispatch repository CONCRETE_INTERFACE testing)

abstract_get_implementations (ABSTRACT repository OUTPUT REPOSITORY_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${REPOSITORY_IMPLEMENTATIONS})
    set (SHARED_NAME "test_repository_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT
            cpu_dispatch=kan cpu_profiler=none error=sdl hash=djb2 log=kan memory=kan memory_profiler=none
            platform=sdl reflection=kan repository=${IMPLEMENTATION} threading=sdl

            CONCRETE container repository_reflection testing test_repository)

    shared_library_verify ()
    shared_library_copy_linked_artefacts ()
    kan_setup_tests (TEST_UNIT test_repository TEST_SHARED_LIBRARY "${SHARED_NAME}")
endforeach ()
