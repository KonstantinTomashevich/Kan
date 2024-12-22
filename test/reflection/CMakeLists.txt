register_concrete (test_reflection)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("generator_test_first.c" "generator_test_second.c" "meta.c" "test_reflection.c")

concrete_preprocessing_queue_step_preprocess ()
reflection_preprocessor_setup_step (DIRECT "generator_test_first.h" "generator_test_second.h" "meta.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT reflection
        CONCRETE_INTERFACE
        testing test_reflection_section_patch_types_pre test_reflection_section_patch_types_post)

# We split section patch types into two units to make generated reflection registration easier.

register_concrete (test_reflection_section_patch_types_pre)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_require (SCOPE PUBLIC ABSTRACT reflection)
concrete_sources_direct ("section_patch_types_pre.c")
concrete_preprocessing_queue_step_preprocess ()
reflection_preprocessor_setup_step (DIRECT "section_patch_types_pre.h")

register_concrete (test_reflection_section_patch_types_post)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_require (SCOPE PUBLIC ABSTRACT reflection)
concrete_sources_direct ("section_patch_types_post.c")
concrete_preprocessing_queue_step_preprocess ()
reflection_preprocessor_setup_step (DIRECT "section_patch_types_post.h")

abstract_get_implementations (ABSTRACT reflection OUTPUT REFLECTION_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${REFLECTION_IMPLEMENTATIONS})
    set (SHARED_NAME "test_reflection_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT
            cpu_dispatch=kan cpu_profiler=default error=sdl hash=djb2 log=kan memory=kan memory_profiler=default
            platform=sdl precise_time=sdl reflection=${IMPLEMENTATION} threading=sdl
            CONCRETE
            container testing test_reflection test_reflection_section_patch_types_pre
            test_reflection_section_patch_types_post)

    shared_library_verify ()
    shared_library_copy_linked_artefacts ()
    kan_setup_tests (TEST_UNIT test_reflection TEST_SHARED_LIBRARY "${SHARED_NAME}")
endforeach ()
