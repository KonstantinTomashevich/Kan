register_concrete (test_resource_pipeline_build)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (
        SCOPE PUBLIC 
        ABSTRACT file_system precise_time stream 
        CONCRETE_INTERFACE context context_reflection_system resource_pipeline_build serialization testing)
setup_reflected_preprocessing ()

register_shared_library (test_resource_pipeline_build_library)
shared_library_include (
        SCOPE PUBLIC
        ABSTRACT
        cpu_dispatch=kan cpu_profiler=default error=sdl file_system=platform_default hash=djb2 log=kan memory=kan 
        memory_profiler=default platform=sdl precise_time=sdl reflection=kan stream=kan threading=sdl
        CONCRETE 
        container context context_reflection_system readable_data reflection_helpers resource_pipeline_build 
        resource_pipeline_runtime resource_pipeline_tooling serialization testing test_resource_pipeline_build)

generate_artefact_context_data ()
generate_artefact_reflection_data ()
shared_library_verify ()
shared_library_copy_linked_artefacts ()

kan_setup_tests (
        TEST_UNIT test_resource_pipeline_build 
        TEST_SHARED_LIBRARY test_resource_pipeline_build_library
        PROPERTIES TIMEOUT 2)
