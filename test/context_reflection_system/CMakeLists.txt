register_concrete (test_context_reflection_system)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

concrete_sources ("*.c")
setup_reflected_preprocessing ()
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE context context_reflection_system testing)

register_context_system (NAME check_generated_system_t PRIVATE)
register_context_system (NAME check_populate_system_t PRIVATE)
register_context_system (NAME check_generation_iterate_system_t PRIVATE)

register_shared_library (test_context_reflection_system_library)
shared_library_include (
        SCOPE PUBLIC
        ABSTRACT
        cpu_dispatch=kan cpu_profiler=default error=sdl hash=djb2 log=kan memory=kan memory_profiler=default
        platform=sdl
        precise_time=sdl reflection=kan threading=sdl
        CONCRETE container context context_reflection_system testing test_context_reflection_system)

generate_artefact_context_data ()
generate_artefact_reflection_data ()
shared_library_verify ()
shared_library_copy_linked_artefacts ()
kan_setup_tests (TEST_UNIT test_context_reflection_system TEST_SHARED_LIBRARY test_context_reflection_system_library)
