# Declares utility functions for settings up units that are built upon on universe unit functionality.

# Configures source for concrete unit that is built upon universe unit.
# Attaches universe preprocessor to all given sources, attaches c interface scanner and reflection generator to all
# headers and universe preprocessor outputs.
# Arguments:
# - DIRECT: list of sources that are specified directly.
# - GLOB: list of patterns that are used to scan for sources.
# - SKIP_REFLECTION_REGISTRATION: if set, register_unit_reflection call is skipped.
function (universe_concrete_setup_sources)
    cmake_parse_arguments (SOURCE "SKIP_REFLECTION_REGISTRATION" "" "DIRECT;GLOB" ${ARGV})
    if (DEFINED SOURCE_UNPARSED_ARGUMENTS OR (NOT DEFINED SOURCE_DIRECT AND NOT DEFINED SOURCE_GLOB))
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    set (INPUT_SOURCES ${SOURCE_DIRECT})
    if (DEFINED SOURCE_GLOB)
        foreach (PATTERN ${SOURCE_GLOB})
            file (GLOB_RECURSE FILES "${PATTERN}")
            list (APPEND INPUT_SOURCES ${FILES})
        endforeach ()
    endif ()

    set (SOURCES)
    set (OUTPUTS)

    set (GENERATED_DIRECTORY "${CMAKE_BINARY_DIR}/generated/${UNIT_NAME}/")
    file (MAKE_DIRECTORY "${GENERATED_DIRECTORY}")

    foreach (INPUT ${INPUT_SOURCES})
        if (NOT IS_ABSOLUTE "${INPUT}")
            set (OUTPUT "${GENERATED_DIRECTORY}/${INPUT}")
            set (INPUT "${CMAKE_CURRENT_SOURCE_DIR}/${INPUT}")
        else ()
            if (NOT INPUT MATCHES "^${CMAKE_CURRENT_SOURCE_DIR}")
                message (SEND_ERROR
                        "Universe concrete unit sources should be under current source dir: ${INPUT}")
            endif ()

            cmake_path (RELATIVE_PATH INPUT BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
            set (OUTPUT "${GENERATED_DIRECTORY}/${INPUT}")
            set (INPUT "${CMAKE_CURRENT_SOURCE_DIR}/${INPUT}")
        endif ()

        list (APPEND SOURCES "${INPUT}")
        list (APPEND OUTPUTS "${OUTPUT}")
    endforeach ()

    foreach (SOURCE OUTPUT IN ZIP_LISTS SOURCES OUTPUTS)
        add_custom_command (
                OUTPUT "${OUTPUT}"
                DEPENDS "${SOURCE}" universe_preprocessor
                COMMENT "Run universe_preprocessor on \"${SOURCE}\"."
                COMMAND universe_preprocessor "${SOURCE}" "${OUTPUT}")
    endforeach ()

    concrete_sources_direct (${OUTPUTS})
    concrete_highlight_direct (${SOURCES})
    c_interface_scanner_setup (GLOB "*.h" DIRECT ${OUTPUTS})
    reflection_generator_setup (GLOB "*.h" DIRECT ${OUTPUTS})

    if (NOT SOURCE_SKIP_REFLECTION_REGISTRATION)
        register_unit_reflection ()
    endif ()

    reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
    concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")
endfunction ()
