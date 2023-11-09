# Contains function for integrating reflection_generator tool into build script.

# Sets variable with given name to path of generated C file with reflection for current unit.
function (reflection_generator_get_output_file_path OUTPUT_VARIABLE_NAME)
    file (MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Generated")
    set ("${OUTPUT_VARIABLE_NAME}" "${CMAKE_CURRENT_BINARY_DIR}/Generated/${UNIT_NAME}_reflection.c" PARENT_SCOPE)
endfunction ()

# Sets up reflection file generation for current unit.
# Arguments:
# - GLOB: Expressions for recurse globbing to search for source files to scan for reflection.
# - DIRECT: Directly specified source files to scan for reflection.
function (reflection_generator_setup)
    cmake_parse_arguments (SETUP "" "" "GLOB;DIRECT" ${ARGV})
    if (DEFINED SETUP_UNPARSED_ARGUMENTS OR (
            NOT DEFINED SETUP_GLOB AND
            NOT DEFINED SETUP_DIRECT))
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    set (INPUTS)
    if (DEFINED SETUP_GLOB)
        foreach (PATTERN ${SETUP_GLOB})
            file (GLOB_RECURSE SOURCES "${PATTERN}")
            list (APPEND INPUTS ${SOURCES})
        endforeach ()
    endif ()

    if (DEFINED SETUP_DIRECT)
        list (APPEND INPUTS ${SETUP_DIRECT})
    endif ()

    c_interface_scanner_to_interface_files (INPUTS)
    reflection_generator_get_output_file_path (OUTPUT_FILE_PATH)

    add_custom_command (
            OUTPUT "${OUTPUT_FILE_PATH}"
            DEPENDS reflection_generator ${INPUTS}

            COMMAND
            reflection_generator
            "${UNIT_NAME}"
            "${OUTPUT_FILE_PATH}"
            ${INPUTS}

            COMMENT "Generate reflection for unit \"${UNIT_NAME}\"."
            COMMAND_EXPAND_LISTS
            VERBATIM)
endfunction ()