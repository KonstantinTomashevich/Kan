register_concrete (test_universe_resource_reference)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

c_interface_scanner_setup (GLOB "*.c")
reflection_generator_setup (GLOB "*.c")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT file_system platform universe_resource_provider universe_resource_reference
        CONCRETE_INTERFACE
        context_reflection_system context_update_system context_virtual_file_system serialization testing universe)

abstract_get_implementations (ABSTRACT universe_resource_reference OUTPUT PROVIDER_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${PROVIDER_IMPLEMENTATIONS})
    set (SHARED_NAME "test_universe_resource_reference_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT
            cpu_dispatch=kan cpu_profiler=none error=sdl file_system=platform_default file_system_watcher=user_level
            hash=djb2 log=kan memory=kan memory_profiler=none platform=sdl reflection=kan repository=kan stream=kan
            threading=sdl universe_resource_provider=kan universe_resource_reference=${IMPLEMENTATION}
            virtual_file_system=kan workflow=kan
            CONCRETE
            container context context_plugin_system context_reflection_system context_update_system
            context_virtual_file_system resource_index resource_pipeline readable_data serialization testing
            test_universe_resource_reference universe)

    generate_artefact_context_data ()
    generate_artefact_reflection_data ()
    shared_library_verify ()
    shared_library_copy_linked_artefacts ()

    kan_setup_tests (
            TEST_UNIT test_universe_resource_reference
            TEST_SHARED_LIBRARY ${SHARED_NAME}
            PROPERTIES TIMEOUT 10)
endforeach ()
