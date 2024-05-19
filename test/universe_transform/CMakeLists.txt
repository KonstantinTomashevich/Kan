register_concrete (test_universe_transform)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

c_interface_scanner_setup (GLOB "*.c")
reflection_generator_setup (GLOB "*.c")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

concrete_require (
        SCOPE PUBLIC
        CONCRETE_INTERFACE
        context_reflection_system context_update_system testing universe universe_single_pipeline_scheduler
        universe_time universe_transform)

register_shared_library (test_universe_transform_library)
shared_library_include (
        SCOPE PUBLIC
        ABSTRACT
        cpu_dispatch=kan cpu_profiler=none error=sdl hash=djb2 log=kan memory=kan memory_profiler=none platform=sdl
        reflection=kan repository=kan threading=sdl workflow=kan
        CONCRETE
        container context context_reflection_system context_update_system inline_math testing test_universe_transform
        universe universe_object universe_single_pipeline_scheduler universe_time universe_transform)

generate_artefact_context_data ()
generate_artefact_reflection_data ()
shared_library_verify ()
shared_library_copy_linked_artefacts ()

kan_setup_tests (
        TEST_UNIT test_universe_transform
        TEST_SHARED_LIBRARY test_universe_transform_library
        PROPERTIES TIMEOUT 2)
