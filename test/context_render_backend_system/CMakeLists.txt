register_concrete (test_context_render_backend_system)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (
        SCOPE PUBLIC
        ABSTRACT context_render_backend_system cpu_dispatch cpu_profiler file_system image
        CONCRETE_INTERFACE inline_math render_pipeline_language testing)
setup_core_preprocessing ()

file (MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests_resources/context_render_backend_system")
add_custom_target (test_context_render_backend_system_copy_expectation
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/expectation" "${CMAKE_BINARY_DIR}/tests_resources/context_render_backend_system"
        COMMENT "Copying context render backend screenshot expectation...")
add_dependencies (test_context_render_backend_system test_context_render_backend_system_copy_expectation)

abstract_get_implementations (
        ABSTRACT context_render_backend_system OUTPUT CONTEXT_RENDER_BACKEND_SYSTEM_IMPLEMENTATIONS)

foreach (IMPLEMENTATION ${CONTEXT_RENDER_BACKEND_SYSTEM_IMPLEMENTATIONS})
    set (SHARED_NAME "test_context_render_backend_system_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT
            context_render_backend_system=${IMPLEMENTATION} cpu_dispatch=kan cpu_profiler=default error=sdl
            file_system=platform_default hash=djb2 image=stb log=kan memory=kan memory_profiler=default platform=sdl
            precise_time=sdl reflection=kan stream=kan threading=sdl
            CONCRETE
            container context context_application_system inline_math render_pipeline_language testing
            test_context_render_backend_system)

    generate_artefact_context_data ()
    shared_library_verify ()
    shared_library_copy_linked_artefacts ()

    kan_setup_tests (
            TEST_UNIT test_context_render_backend_system
            NO_ASAN_LEAK_TEST
            TEST_SHARED_LIBRARY "${SHARED_NAME}"
            # We need more timeout due to initialization/shutdown times.
            PROPERTIES TIMEOUT 5 LABELS GRAPHICS)
endforeach ()
