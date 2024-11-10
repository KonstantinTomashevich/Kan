register_concrete (test_render_pipeline_language)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT file_system CONCRETE_INTERFACE render_pipeline_language testing)

file (MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests_resources/render_pipeline_language")
add_custom_target (test_render_pipeline_language_copy_pipelines
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/pipelines" "${CMAKE_BINARY_DIR}/tests_resources/render_pipeline_language"
        COMMENT "Copying render pipeline language test pipelines...")
add_dependencies (test_render_pipeline_language test_render_pipeline_language_copy_pipelines)

register_shared_library (test_render_pipeline_language_library)
shared_library_include (
        SCOPE PUBLIC
        ABSTRACT
        cpu_dispatch=kan cpu_profiler=none error=sdl file_system=platform_default hash=djb2 log=kan memory=kan
        memory_profiler=none platform=sdl reflection=kan stream=kan threading=sdl
        CONCRETE container render_pipeline_language testing test_render_pipeline_language)

shared_library_verify ()
shared_library_copy_linked_artefacts ()

# We need higher timeout for benchmark test.
set (TEST_TIMEOUT 10)

if (KAN_ENABLE_ADDRESS_SANITIZER OR WIN32)
    # And with address sanitizer benchmark test is very long.
    # For some reason, on Windows in debug mode allocations are also super expensive, therefore we need bigger timeout.
    set (TEST_TIMEOUT 60)
endif ()

kan_setup_tests (
        TEST_UNIT test_render_pipeline_language TEST_SHARED_LIBRARY test_render_pipeline_language_library
        PROPERTIES TIMEOUT ${TEST_TIMEOUT})
