register_concrete (test_universe_transform)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (GLOB "*.c")

concrete_require (
        SCOPE PUBLIC
        CONCRETE_INTERFACE
        context_reflection_system context_update_system testing universe universe_trivial_scheduler universe_transform)
setup_reflected_preprocessing ()

register_shared_library (test_universe_transform_library)
shared_library_include (
        SCOPE PUBLIC
        ABSTRACT
        cpu_dispatch=kan cpu_profiler=default error=sdl hash=djb2 log=kan memory=kan memory_profiler=default
        platform=sdl precise_time=sdl reflection=kan repository=kan threading=sdl workflow=kan
        CONCRETE
        container context context_reflection_system context_update_system inline_math testing test_universe_transform
        universe universe_object universe_trivial_scheduler universe_transform)

generate_artefact_context_data ()
generate_artefact_reflection_data ()
shared_library_verify ()
shared_library_copy_linked_artefacts ()

kan_setup_tests (
        TEST_SHARED_LIBRARY test_universe_transform_library
        TEST_SOURCES "test_universe_transform.c"
        PROPERTIES TIMEOUT 2)
