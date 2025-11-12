register_concrete (test_text)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (
        SCOPE PUBLIC
        ABSTRACT context_render_backend_system file_system image precise_time text
        CONCRETE_INTERFACE inline_math render_pipeline_language testing)
setup_core_preprocessing ()

file (MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests_resources/text")
add_custom_target (test_text_copy_resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_BINARY_DIR}/tests_resources/text"
        COMMENT "Copying text test resources...")
add_dependencies (test_text test_text_copy_resources)

abstract_get_implementations (ABSTRACT text OUTPUT TEXT_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${TEXT_IMPLEMENTATIONS})
    set (SHARED_NAME "test_text_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT
            context_render_backend_system=vulkan cpu_dispatch=kan cpu_profiler=default error=sdl
            file_system=platform_default hash=djb2 image=stb log=kan memory=kan memory_profiler=default platform=sdl
            precise_time=sdl reflection=kan stream=kan text=${IMPLEMENTATION} threading=sdl
            CONCRETE
            container context context_application_system inline_math render_pipeline_language testing test_text)

    generate_artefact_context_data ()
    shared_library_verify ()
    shared_library_copy_linked_artefacts ()

    kan_setup_tests (
            TEST_UNIT test_text
            NO_ASAN_LEAK_TEST
            TEST_SHARED_LIBRARY "${SHARED_NAME}"
            # We need more timeout due to initialization/shutdown times.
            PROPERTIES TIMEOUT 5 LABELS GRAPHICS)
endforeach ()
