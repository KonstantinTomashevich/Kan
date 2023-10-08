set (C_INTERFACE_SCANNER_WORKSPACE "${CMAKE_BINARY_DIR}/c_interface_scanner_workspace")

# TODO: It is temporary stub to start testing c_interface_scanner.

add_custom_target (scan_everything)

function (c_interface_scanner_add_to_unit)
    get_unit_api_variables ("${UNIT_NAME}")

    # TODO: It is a very temporary stub. Find good way to get sources.
    file (GLOB_RECURSE UNIT_SOURCES "*.h" "*.c")

    foreach (SOURCE ${UNIT_SOURCES})
        set (RELATIVE_SOURCE "${SOURCE}")
        cmake_path (RELATIVE_PATH RELATIVE_SOURCE BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

        set (OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/${RELATIVE_SOURCE}.interface")
        cmake_path (GET OUTPUT_PATH PARENT_PATH OUTPUT_PATH_PARENT)
        file (MAKE_DIRECTORY "${OUTPUT_PATH_PARENT}")

        add_custom_command (
                OUTPUT "${OUTPUT_PATH}"
                DEPENDS c_interface_scanner "${SOURCE}"

                COMMAND
                c_interface_scanner
                "${UNIT_API_MACRO}"
                "${SOURCE}"
                "${OUTPUT_PATH}"

                COMMENT "Scan interface of \"${SOURCE}\"."
                VERBATIM)

        list (APPEND OUTPUTS "${OUTPUT_PATH}")
    endforeach ()

    add_custom_target ("scan_${UNIT_NAME}" DEPENDS ${OUTPUTS} COMMENT "Scan all sources of \"${UNIT_NAME}\".")
    add_dependencies (scan_everything "scan_${UNIT_NAME}")
endfunction ()
