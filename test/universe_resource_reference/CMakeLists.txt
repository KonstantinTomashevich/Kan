register_concrete (test_universe_resource_reference)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
universe_concrete_setup_sources (GLOB "*.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT
        context_hot_reload_coordination_system file_system platform precise_time universe_resource_provider
        universe_resource_reference
        CONCRETE_INTERFACE
        context_reflection_system context_resource_pipeline_system context_update_system context_virtual_file_system
        serialization testing universe)

abstract_get_implementations (ABSTRACT universe_resource_reference OUTPUT PROVIDER_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${PROVIDER_IMPLEMENTATIONS})
    set (SHARED_NAME "test_universe_resource_reference_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT
            context_hot_reload_coordination_system=kan cpu_dispatch=kan cpu_profiler=default error=sdl
            file_system=platform_default file_system_watcher=user_level  hash=djb2 log=kan memory=kan
            memory_profiler=default platform=sdl precise_time=sdl reflection=kan repository=kan stream=kan threading=sdl
            universe_resource_provider=kan universe_resource_reference=${IMPLEMENTATION} virtual_file_system=kan
            workflow=kan
            CONCRETE
            container context context_application_system context_plugin_system context_reflection_system
            context_resource_pipeline_system context_update_system context_virtual_file_system reflection_helpers
            resource_index resource_pipeline readable_data serialization testing test_universe_resource_reference
            universe)

    generate_artefact_context_data ()
    generate_artefact_reflection_data ()
    shared_library_verify ()
    shared_library_copy_linked_artefacts ()

    kan_setup_tests (
            TEST_SHARED_LIBRARY ${SHARED_NAME}
            TEST_SOURCES "test_universe_resource_reference.c"
            PROPERTIES TIMEOUT 10)
endforeach ()
