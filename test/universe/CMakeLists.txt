register_concrete (test_universe)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (DIRECT "test_universe.c")
register_context_system (NAME migration_reflection_population_system_t PRIVATE)
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE context_reflection_system context_update_system testing universe)
universe_concrete_preprocessing_queue ()

register_concrete (test_universe_pre_migration)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (DIRECT "test_universe_pre_migration.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE testing universe)
universe_concrete_preprocessing_queue (SKIP_REFLECTION_REGISTRATION)

register_concrete (test_universe_post_migration)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (DIRECT "test_universe_post_migration.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE testing universe)
universe_concrete_preprocessing_queue (SKIP_REFLECTION_REGISTRATION)

register_shared_library (test_universe_library)
shared_library_include (
        SCOPE PUBLIC
        ABSTRACT
        cpu_dispatch=kan cpu_profiler=default error=sdl hash=djb2 log=kan memory=kan memory_profiler=default
        platform=sdl precise_time=sdl reflection=kan repository=kan threading=sdl workflow=kan
        CONCRETE
        container context context_reflection_system context_update_system testing test_universe
        test_universe_post_migration test_universe_pre_migration universe)

generate_artefact_context_data ()
generate_artefact_reflection_data ()
shared_library_verify ()
shared_library_copy_linked_artefacts ()
kan_setup_tests (TEST_SHARED_LIBRARY test_universe_library TEST_SOURCES "test_universe.c" PROPERTIES TIMEOUT 2)
