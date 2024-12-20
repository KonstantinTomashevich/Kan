# Contains functions for setting up reflection generation through reflection preprocessor tool.

# Configures reflection preprocessor step for concrete preprocessing queue.
# Reflection preprocessor is executed as gather step and creates reflection code as byproduct.
# `concrete_preprocessing_queue_step_preprocess` must be executed before using reflection preprocessor.
# Reflection preprocessor searches for given source files inside preprocessed data and extracts reflection from them.
# Source code that is not from given source files is ignored.
#
# Arguments:
# - OVERRIDE_UNIT_NAME: optional argument that allows to override unit name inside generated reflection data.
# - GLOB: GLOB expressions to find files from which reflection data should be extracted.
# - DIRECT: Direct list of files from which reflection data should be extracted.
function (reflection_preprocessor_setup_step)
    cmake_parse_arguments (ARG "" "OVERRIDE_UNIT_NAME" "GLOB;DIRECT" ${ARGV})
    if (DEFINED ARG_UNPARSED_ARGUMENTS OR (
            NOT DEFINED ARG_GLOB AND
            NOT DEFINED ARG_DIRECT))
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    set (CURRENT_UNIT_NAME "${UNIT_NAME}")
    if (DEFINED ARG_OVERRIDE_UNIT_NAME)
        set (CURRENT_UNIT_NAME "${ARG_OVERRIDE_UNIT_NAME}")
    endif ()

    set (INPUTS)
    if (DEFINED ARG_GLOB)
        foreach (PATTERN ${ARG_GLOB})
            file (GLOB_RECURSE SOURCES "${PATTERN}")
            foreach (SOURCE ${SOURCES})
                cmake_path (ABSOLUTE_PATH SOURCE)
                cmake_path (NATIVE_PATH SOURCE NORMALIZE NATIVE_SOURCE)
                list (APPEND INPUTS "${NATIVE_SOURCE}")
            endforeach ()
        endforeach ()
    endif ()

    if (DEFINED ARG_DIRECT)
        foreach (SOURCE ${ARG_DIRECT})
            cmake_path (ABSOLUTE_PATH SOURCE)
            cmake_path (NATIVE_PATH SOURCE NORMALIZE NATIVE_SOURCE)
            list (APPEND INPUTS "${NATIVE_SOURCE}")
        endforeach ()
    endif ()

    list (JOIN INPUTS "\n" INPUTS_LIST_CONTENT)
    set (LIST_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/${CURRENT_UNIT_NAME}.reflection.list")
    file (WRITE "${LIST_FILE_NAME}" "${INPUTS_LIST_CONTENT}")

    concrete_preprocessing_queue_step_gather (
            COMMAND reflection_preprocessor
            PRODUCT "reflection.c"
            ARGUMENTS
            "$$PRODUCT" "${CURRENT_UNIT_NAME}" "${LIST_FILE_NAME}" "$$INPUT_LIST"
            EXTRA_DEPENDENCIES "${LIST_FILE_NAME}")
endfunction ()
