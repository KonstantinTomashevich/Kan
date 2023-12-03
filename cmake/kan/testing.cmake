# Declares function for automatic test extraction into separate executables and their registration inside CTest.

# Path to test runner template file.
set (KAN_TEST_RUNNER_TEMPLATE "${CMAKE_SOURCE_DIR}/cmake/kan/test_runner.c")

# Setup target to build all tests.
add_custom_target (test_kan COMMENT "All tests must be dependencies of this target.")

# Setups CTest test runners by scanning test unit sources and generating simple runner executables.
# Arguments:
# - TEST_UNIT: name of the unit that contains tests to be executed. Sources are scanned for KAN_TEST_CASE macro.
# - TEST_SHARED_LIBRARY: name of the shared library to which test runner must link.
# - PROPERTIES: value of this argument is redirect to set_tests_properties for every generated test.
function (kan_setup_tests)
    cmake_parse_arguments (SETUP "" "TEST_UNIT;TEST_SHARED_LIBRARY" "PROPERTIES" ${ARGV})
    if (DEFINED SETUP_UNPARSED_ARGUMENTS OR
            NOT DEFINED SETUP_TEST_UNIT OR
            NOT DEFINED SETUP_TEST_SHARED_LIBRARY)
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    message (STATUS "Setting up tests from \"${SETUP_TEST_UNIT}\" using \"${SETUP_TEST_SHARED_LIBRARY}\" library.")
    get_target_property (TEST_SOURCES "${SETUP_TEST_UNIT}" SOURCES)
    file (MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Generated")

    foreach (TEST_SOURCE ${TEST_SOURCES})
        if (NOT EXISTS "${TEST_SOURCE}")
            continue ()
        endif ()

        file (STRINGS "${TEST_SOURCE}" TEST_SOURCE_LINES)
        foreach (TEST_LINE ${TEST_SOURCE_LINES})
            if (TEST_LINE MATCHES "^KAN_TEST_CASE.*\\((.+)\\)$")
                set (TEST_NAME "${CMAKE_MATCH_1}")
                message (STATUS "    Setting up test case \"${TEST_NAME}\" from source \"${TEST_SOURCE}\".")

                set (TEST_RUNNER_FILE "${CMAKE_CURRENT_BINARY_DIR}/Generated/test_runner_${TEST_NAME}.c")
                configure_file ("${KAN_TEST_RUNNER_TEMPLATE}" "${TEST_RUNNER_FILE}")

                set (TEST_RUNNER_TARGET "test_runner_${SETUP_TEST_SHARED_LIBRARY}_case_${TEST_NAME}")
                # We use cmake directly to avoid overcomplicating tests with CMakeUnitFramework executables.

                add_executable ("${TEST_RUNNER_TARGET}" "${TEST_RUNNER_FILE}")
                target_link_libraries ("${TEST_RUNNER_TARGET}" PUBLIC "${SETUP_TEST_SHARED_LIBRARY}" SDL3::SDL3)

                set (TEST_WORKSPACE "${CMAKE_CURRENT_BINARY_DIR}/Workspace/${TEST_NAME}")
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
endfunction ()
