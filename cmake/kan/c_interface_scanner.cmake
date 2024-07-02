# Contains function for integrating c_interface_scanner tool into build script.

# Intended only for internal use in this file. Generates output directory for c_interface_scanner.
function (private_c_interface_scanner_get_directory)
    set (C_INTERFACE_SCANNER_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/c_interface_scanner" PARENT_SCOPE)
endfunction ()

# Enables c_interface_scanner commands for files inside current unit.
# Arguments:
# - GLOB: Expressions for recurse globbing to search for source files to include into scanning list.
# - DIRECT: Directly specified source files to include into scanning list.
function (c_interface_scanner_setup)
    cmake_parse_arguments (SETUP "" "" "GLOB;DIRECT" ${ARGV})
    if (DEFINED SETUP_UNPARSED_ARGUMENTS OR (
            NOT DEFINED SETUP_GLOB AND
            NOT DEFINED SETUP_DIRECT))
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    set (FILES_TO_SCAN)
    if (DEFINED SETUP_GLOB)
        foreach (PATTERN ${SETUP_GLOB})
            file (GLOB_RECURSE SOURCES "${PATTERN}")
            list (APPEND FILES_TO_SCAN ${SOURCES})
        endforeach ()
    endif ()

    if (DEFINED SETUP_DIRECT)
        foreach (DIRECT ${SETUP_DIRECT})
            if (NOT IS_ABSOLUTE "${DIRECT}")
                set (DIRECT "${CMAKE_CURRENT_SOURCE_DIR}/${DIRECT}")
            endif ()

            list (APPEND FILES_TO_SCAN "${DIRECT}")
        endforeach ()
    endif ()

    private_c_interface_scanner_get_directory ()
    file (MAKE_DIRECTORY ${C_INTERFACE_SCANNER_DIRECTORY})
    get_unit_api_variables ("${UNIT_NAME}")

    foreach (FILE_TO_SCAN ${FILES_TO_SCAN})
        set (ABSOLUTE_SOURCE "${FILE_TO_SCAN}")
        cmake_path (ABSOLUTE_PATH ABSOLUTE_SOURCE)

        if (ABSOLUTE_SOURCE MATCHES "^${CMAKE_CURRENT_SOURCE_DIR}")
            set (RELATIVE_SOURCE "${FILE_TO_SCAN}")
            cmake_path (RELATIVE_PATH RELATIVE_SOURCE BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        elseif (ABSOLUTE_SOURCE MATCHES "^${CMAKE_CURRENT_BINARY_DIR}/Generated")
            set (RELATIVE_SOURCE "${FILE_TO_SCAN}")
            cmake_path (RELATIVE_PATH RELATIVE_SOURCE BASE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Generated")
        else ()
            message (SEND_ERROR "c_interface_scanner failed to determinate output path for \"${FILE_TO_SCAN}\".")
        endif ()

        set (OUTPUT_PATH "${C_INTERFACE_SCANNER_DIRECTORY}/${RELATIVE_SOURCE}.interface")
        cmake_path (GET OUTPUT_PATH PARENT_PATH OUTPUT_PATH_PARENT)
        file (MAKE_DIRECTORY "${OUTPUT_PATH_PARENT}")

        add_custom_command (
                OUTPUT "${OUTPUT_PATH}"
                DEPENDS c_interface_scanner "${ABSOLUTE_SOURCE}"

                COMMAND
                c_interface_scanner
                "${UNIT_API_MACRO}"
                "${ABSOLUTE_SOURCE}"
                "${OUTPUT_PATH}"

                COMMENT "Scan interface of \"${ABSOLUTE_SOURCE}\"."
                VERBATIM)
    endforeach ()
endfunction ()

# Converts all paths to source files of current unit inside list with given name to paths to their scanned interfaces.
function (c_interface_scanner_to_interface_files INPUT_VARIABLE_NAME)
    private_c_interface_scanner_get_directory ()
    set (RESULTS)

    foreach (FILE ${${INPUT_VARIABLE_NAME}})
        if (NOT IS_ABSOLUTE "${FILE}")
            set (FILE "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}")
        endif ()

        if (FILE MATCHES "^${CMAKE_CURRENT_SOURCE_DIR}")
            cmake_path (RELATIVE_PATH FILE BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        elseif (FILE MATCHES "^${CMAKE_CURRENT_BINARY_DIR}/Generated")
            cmake_path (RELATIVE_PATH FILE BASE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Generated")
        else ()
            message (SEND_ERROR "c_interface_scanner failed to determinate output path for \"${FILE}\".")
        endif ()

        set (FILE "${C_INTERFACE_SCANNER_DIRECTORY}/${FILE}.interface")
        list (APPEND RESULTS "${FILE}")
    endforeach ()

    set ("${INPUT_VARIABLE_NAME}" ${RESULTS} PARENT_SCOPE)
endfunction ()
