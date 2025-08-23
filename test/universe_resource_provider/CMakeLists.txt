register_concrete (test_universe_resource_provider)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (GLOB "*.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT context_hot_reload_coordination_system file_system platform precise_time universe_resource_provider
        CONCRETE_INTERFACE
        context_reflection_system context_update_system context_virtual_file_system resource_pipeline_build
        serialization testing universe)
setup_reflected_preprocessing ()

register_shared_library (test_universe_resource_provider_library)
shared_library_include (
        SCOPE PUBLIC
        ABSTRACT
        context_hot_reload_coordination_system=kan cpu_dispatch=kan cpu_profiler=default error=sdl
        file_system=platform_default file_system_watcher=user_level hash=djb2 log=kan memory=kan
        memory_profiler=default platform=sdl precise_time=sdl reflection=kan repository=kan stream=kan
        threading=sdl virtual_file_system=kan workflow=kan
        CONCRETE
        container context context_application_system context_reflection_system context_update_system 
        context_virtual_file_system reflection_helpers resource_pipeline_build resource_pipeline_runtime
        resource_pipeline_tooling readable_data serialization testing test_universe_resource_provider universe 
        universe_resource_provider)

generate_artefact_context_data ()
generate_artefact_reflection_data ()
shared_library_verify ()
shared_library_copy_linked_artefacts ()

kan_setup_tests (
        TEST_SHARED_LIBRARY test_universe_resource_provider_library 
        TEST_SOURCES "test_universe_resource_provider.c" 
        PROPERTIES TIMEOUT 2)
