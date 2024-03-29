register_concrete (test_context)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE context testing)

register_context_system (first_independent_system_t)
register_context_system (second_independent_system_t)
register_context_system (system_with_dependencies_t)

register_shared_library (test_context_library)
shared_library_include (
        SCOPE PUBLIC
        ABSTRACT
        error=sdl hash=djb2 log=kan memory=kan memory_profiler=none platform=sdl threading=sdl
        CONCRETE container context testing test_context)

generate_artefact_context_data ()
shared_library_verify ()
shared_library_copy_linked_artefacts ()
kan_setup_tests (TEST_UNIT test_context TEST_SHARED_LIBRARY test_context_library)
