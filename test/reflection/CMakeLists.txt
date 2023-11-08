register_concrete (test_reflection)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

c_interface_scanner_setup (GLOB "*.h" DIRECT "meta.c")
reflection_generator_setup (GLOB "*.h" DIRECT "meta.c")

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

concrete_require (SCOPE PUBLIC ABSTRACT reflection CONCRETE_INTERFACE testing)

abstract_get_implementations (ABSTRACT reflection OUTPUT MEMORY_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${MEMORY_IMPLEMENTATIONS})
    set (SHARED_NAME "test_reflection_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT
            error=sdl hash=djb2 log=kan memory=kan memory_profiler=none reflection=${IMPLEMENTATION} threading=sdl

            CONCRETE container testing test_reflection)

    shared_library_verify ()
    shared_library_copy_linked_artefacts ()
    kan_setup_tests (TEST_UNIT test_reflection TEST_SHARED_LIBRARY "${SHARED_NAME}")
endforeach ()
