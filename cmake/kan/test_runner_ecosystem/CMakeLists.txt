# Ecosystem CMake script for test runners from testing.cmake in parent directory.

foreach (TEST_SOURCE ${TEST_SOURCES})
    if (NOT IS_ABSOLUTE "${TEST_SOURCE}")
        set (TEST_SOURCE "${TEST_SOURCE_DIR}/${TEST_SOURCE}")
    endif ()

    if (NOT EXISTS "${TEST_SOURCE}" OR "${TEST_SOURCE}" MATCHES ".*/generated/.*")
        continue ()
    endif ()

    file (STRINGS "${TEST_SOURCE}" TEST_SOURCE_LINES)
    set (TEST_RUNNER_INDEX 0)

    foreach (TEST_LINE ${TEST_SOURCE_LINES})
        if (TEST_LINE MATCHES "^KAN_TEST_CASE.*\\((.+)\\)$")
            set (TEST_NAME "${CMAKE_MATCH_1}")
            message (STATUS "    Setting up test case \"${TEST_NAME}\" from source \"${TEST_SOURCE}\".")

            set (TEST_RUNNER_FILE "${GENERATED_DIRECTORY}/test_runner_${TEST_RUNNER_INDEX}.c")
            math (EXPR TEST_RUNNER_INDEX "${TEST_RUNNER_INDEX} + 1")
            configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/test_runner.c" "${TEST_RUNNER_FILE}")

            set (TEST_RUNNER_TARGET "${SETUP_TEST_SHARED_LIBRARY}_case_${TEST_NAME}")
            # We use cmake directly to avoid overcomplicating tests with CMakeUnitFramework executables.

            add_executable ("${TEST_RUNNER_TARGET}" "${TEST_RUNNER_FILE}")
            target_link_libraries ("${TEST_RUNNER_TARGET}"
                    PUBLIC "${SETUP_TEST_SHARED_LIBRARY}" error error_sdl SDL3::SDL3)
            set_target_properties ("${TEST_RUNNER_TARGET}" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_BINARY_DIR}")

            set (TEST_WORKSPACE "${WORKSPACE_DIRECTORY}/${TEST_NAME}")
            file (MAKE_DIRECTORY "${TEST_WORKSPACE}")

            add_test (
                    NAME "${SETUP_TEST_SHARED_LIBRARY}_case_${TEST_NAME}"
                    COMMAND "${TEST_RUNNER_TARGET}"
                    WORKING_DIRECTORY "${TEST_WORKSPACE}")
            add_dependencies (test_kan "${TEST_RUNNER_TARGET}")

            if (DEFINED SETUP_PROPERTIES)
                set_tests_properties (
                        "${SETUP_TEST_SHARED_LIBRARY}_case_${TEST_NAME}" PROPERTIES ${SETUP_PROPERTIES})
            endif ()

        endif ()
    endforeach ()
endforeach ()
