register_concrete (test_reflection)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("generator_test_first.c" "generator_test_second.c" "meta.c" "test_reflection.c")

c_interface_scanner_setup (DIRECT "generator_test_first.h" "generator_test_second.h" "meta.c")
reflection_generator_setup (DIRECT "generator_test_first.h" "generator_test_second.h" "meta.c")

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT reflection
        CONCRETE_INTERFACE
        testing test_reflection_section_patch_types_pre test_reflection_section_patch_types_post)

# We split section patch types into two units to make generated reflection registration easier.

register_concrete (test_reflection_section_patch_types_pre)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_require (SCOPE PUBLIC ABSTRACT reflection)

c_interface_scanner_setup (DIRECT "section_patch_types_pre.h")
reflection_generator_setup (DIRECT "section_patch_types_pre.h")

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}" "section_patch_types_pre.c")

register_concrete (test_reflection_section_patch_types_post)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_require (SCOPE PUBLIC ABSTRACT reflection)

c_interface_scanner_setup (DIRECT "section_patch_types_post.h")
reflection_generator_setup (DIRECT "section_patch_types_post.h")

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}" "section_patch_types_post.c")

abstract_get_implementations (ABSTRACT reflection OUTPUT REFLECTION_IMPLEMENTATIONS)
foreach (IMPLEMENTATION ${REFLECTION_IMPLEMENTATIONS})
    set (SHARED_NAME "test_reflection_${IMPLEMENTATION}")
    register_shared_library (${SHARED_NAME})

    shared_library_include (
            SCOPE PUBLIC
            ABSTRACT
            cpu_dispatch=kan cpu_profiler=none error=sdl hash=djb2 log=kan memory=kan memory_profiler=none
            platform=sdl reflection=${IMPLEMENTATION} threading=sdl

            CONCRETE
            container testing test_reflection test_reflection_section_patch_types_pre
            test_reflection_section_patch_types_post)

    shared_library_verify ()
    shared_library_copy_linked_artefacts ()
    kan_setup_tests (TEST_UNIT test_reflection TEST_SHARED_LIBRARY "${SHARED_NAME}")
endforeach ()
