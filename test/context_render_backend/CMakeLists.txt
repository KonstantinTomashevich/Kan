register_concrete (test_context_render_backend_system)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT context_render_backend_system CONCRETE_INTERFACE testing)

abstract_get_implementations (
        ABSTRACT context_render_backend_system OUTPUT CONTEXT_RENDER_BACKEND_SYSTEM_IMPLEMENTATIONS)

foreach (IMPLEMENTATION ${CONTEXT_RENDER_BACKEND_SYSTEM_IMPLEMENTATIONS})
    set (SHARED_NAME "test_context_render_backend_system_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT
            context_render_backend_system=${IMPLEMENTATION} cpu_dispatch=kan cpu_profiler=none error=sdl hash=djb2
            log=kan memory=kan memory_profiler=none platform=sdl reflection=kan threading=sdl
            CONCRETE container context context_application_system testing test_context_render_backend_system)

    generate_artefact_context_data ()
    shared_library_verify ()
    shared_library_copy_linked_artefacts ()
    kan_setup_tests (TEST_UNIT test_context_render_backend_system TEST_SHARED_LIBRARY "${SHARED_NAME}")
endforeach ()
