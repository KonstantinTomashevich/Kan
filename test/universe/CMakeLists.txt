register_concrete (test_universe)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

c_interface_scanner_setup (GLOB "test_universe.c")
reflection_generator_setup (DIRECT "test_universe.c")
register_unit_reflection ()
register_context_system (migration_reflection_population_system_t)

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/test_universe.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE context_reflection_system context_update_system testing universe)

register_concrete (test_universe_pre_migration)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

c_interface_scanner_setup (GLOB "test_universe_pre_migration.c")
reflection_generator_setup (DIRECT "test_universe_pre_migration.c")

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_universe_pre_migration.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE testing universe)

register_concrete (test_universe_post_migration)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

c_interface_scanner_setup (GLOB "test_universe_post_migration.c")
reflection_generator_setup (DIRECT "test_universe_post_migration.c")

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_universe_post_migration.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE testing universe)

register_shared_library (test_universe_library)
shared_library_include (
        SCOPE PUBLIC
        ABSTRACT
        cpu_dispatch=kan cpu_profiler=none error=sdl hash=djb2 log=kan memory=kan memory_profiler=none platform=sdl
        reflection=kan repository=kan threading=sdl workflow=kan
        CONCRETE
        container context context_reflection_system context_update_system testing test_universe
        test_universe_post_migration test_universe_pre_migration universe)

generate_artefact_context_data ()
generate_artefact_reflection_data ()
shared_library_verify ()
shared_library_copy_linked_artefacts ()
kan_setup_tests (TEST_UNIT test_universe TEST_SHARED_LIBRARY test_universe_library PROPERTIES TIMEOUT 2)
