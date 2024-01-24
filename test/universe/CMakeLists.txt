register_concrete (test_universe)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

c_interface_scanner_setup (GLOB "*.c")
reflection_generator_setup (DIRECT "test_universe.c")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/test_universe.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE context_reflection_system context_update_system testing universe)

register_shared_library (test_universe_library)
shared_library_include (
        SCOPE PUBLIC
        ABSTRACT
        cpu_dispatch=kan cpu_profiler=none error=sdl hash=djb2 log=kan memory=kan memory_profiler=none platform=sdl
        reflection=kan repository=kan threading=sdl workflow=kan
        CONCRETE
        container context context_reflection_system context_update_system testing repository_reflection
        test_universe universe)

generate_artefact_context_data ()
generate_artefact_reflection_data ()
shared_library_verify ()
shared_library_copy_linked_artefacts ()
kan_setup_tests (TEST_UNIT test_universe TEST_SHARED_LIBRARY test_universe_library PROPERTIES TIMEOUT 2)
