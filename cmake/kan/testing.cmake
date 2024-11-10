# Declares function for automatic test extraction into separate executables and their registration inside CTest.

# Path to test runner ecosystem subdirectory.
# Test runners are split into ecosystems in order to put them into flattened binary directories.
set (KAN_TEST_RUNNER_ECOSYSTEM "${CMAKE_SOURCE_DIR}/cmake/kan/test_runner_ecosystem")

# Setup target to build all tests.
add_custom_target (test_kan COMMENT "All tests must be dependencies of this target.")

# Setups CTest test runners by scanning test unit sources and generating simple runner executables.
# Arguments:
# - TEST_UNIT: name of the unit that contains tests to be executed. Sources are scanned for KAN_TEST_CASE macro.
#              This argument is only used when sources are not directly specified through TEST_SOURCES argument.
# - TEST_SHARED_LIBRARY: name of the shared library to which test runner must link.
# - TEST_SOURCES: list of sources to scan for KAN_TEST_CASE macro. Overrides TEST_UNIT argument.
# - PROPERTIES: value of this argument is redirect to set_tests_properties for every generated test.
function (kan_setup_tests)
    cmake_parse_arguments (SETUP "" "TEST_UNIT;TEST_SHARED_LIBRARY" "TEST_SOURCES;PROPERTIES" ${ARGV})
    if (DEFINED SETUP_UNPARSED_ARGUMENTS OR (
            NOT DEFINED SETUP_TEST_UNIT AND NOT DEFINED SETUP_TEST_SOURCES) OR
            NOT DEFINED SETUP_TEST_SHARED_LIBRARY)
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    if (NOT DEFINED SETUP_TEST_SOURCES)
        message (STATUS "Setting up tests from \"${SETUP_TEST_UNIT}\" using \"${SETUP_TEST_SHARED_LIBRARY}\" library.")
        get_target_property (TEST_SOURCES "${SETUP_TEST_UNIT}" SOURCES)
    else ()
        message (STATUS "Setting up tests using \"${SETUP_TEST_SHARED_LIBRARY}\" library.")
        set (TEST_SOURCES ${SETUP_TEST_SOURCES})
    endif ()

    set (GENERATED_DIRECTORY "${CMAKE_BINARY_DIR}/generated/${SETUP_TEST_SHARED_LIBRARY}")
    file (MAKE_DIRECTORY "${GENERATED_DIRECTORY}")

    set (WORKSPACE_DIRECTORY "${CMAKE_BINARY_DIR}/workspace/${SETUP_TEST_SHARED_LIBRARY}/")
    file (MAKE_DIRECTORY ${WORKSPACE_DIRECTORY})

    set (TEST_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set (TEST_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    get_next_flattened_binary_directory (TEMP_DIRECTORY)
    add_subdirectory ("${KAN_TEST_RUNNER_ECOSYSTEM}" "${TEMP_DIRECTORY}")
endfunction ()
