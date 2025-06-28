# Declares utility functions for preprocessing setup for different units.

# Setups minimal preprocessing module for the unit that belongs to foundation core.
function (setup_core_preprocessing)
    concrete_preprocessing_queue_step_cushion ()
endfunction ()

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
                list (APPEND INPUTS "${SOURCE}")
            endforeach ()
        endforeach ()
    endif ()

    if (DEFINED ARG_DIRECT)
        foreach (SOURCE ${ARG_DIRECT})
            cmake_path (ABSOLUTE_PATH SOURCE)
            list (APPEND INPUTS "${SOURCE}")
        endforeach ()
    endif ()

    list (JOIN INPUTS "\n" INPUTS_LIST_CONTENT)
    set (LIST_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/${CURRENT_UNIT_NAME}.reflection.list")
    file_write_if_not_equal ("${LIST_FILE_NAME}" "${INPUTS_LIST_CONTENT}")

    concrete_preprocessing_queue_step_gather (
            COMMAND reflection_preprocessor
            PRODUCT "reflection.c"
            ARGUMENTS
            "$$PRODUCT" "${CURRENT_UNIT_NAME}" "${LIST_FILE_NAME}" "$$INPUT_LIST"
            EXTRA_DEPENDENCIES "${LIST_FILE_NAME}")
endfunction ()

# Setups preprocessing queue for concrete unit that uses reflection.
# Arguments:
# - SKIP_REFLECTION_REGISTRATION: Optional flag. If passed, unit reflection is not registered. Mostly used for tests.
# - REFLECTION_GLOB: Optional multi value argument, glob expression for searching for sources to scan.
#                    If neither REFLECTION_GLOB nor REFLECTION_DIRECT is specified, GLOB with "*.h" "*.c" is used.
# - REFLECTION_DIRECT: Optional multi value argument, list of direct sources to scan for reflection.
function (setup_reflected_preprocessing)
    cmake_parse_arguments (ARG "SKIP_REFLECTION_REGISTRATION" "" "REFLECTION_GLOB;REFLECTION_DIRECT" ${ARGV})
    if (DEFINED ARG_UNPARSED_ARGUMENTS)
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    concrete_preprocessing_queue_step_cushion ()
    set (PATTERNS)
    
    if (DEFINED ARG_REFLECTION_GLOB)
        list (APPEND PATTERNS "GLOB" ${ARG_REFLECTION_GLOB})
    endif ()

    if (DEFINED ARG_REFLECTION_DIRECT)
        list (APPEND PATTERNS "DIRECT" ${ARG_REFLECTION_DIRECT})
    endif ()

    if (NOT DEFINED ARG_REFLECTION_GLOB AND NOT DEFINED ARG_REFLECTION_DIRECT)
        list (APPEND PATTERNS "GLOB" "*.h" "*.c")
    endif ()
    
    reflection_preprocessor_setup_step (${PATTERNS})
    if (NOT ARG_SKIP_REFLECTION_REGISTRATION)
        register_unit_reflection ()
    endif ()
endfunction ()

# Sets up concrete unit with reflection for an abstract unit.
# Abstract units do not have sources and therefore do not have their own reflection. But sometimes reflection is needed.
# In this case, special concrete unit is created and added to all the implementations. This function is the advised
# way of creating this accompanying reflection unit.
#
# Arguments:
# - FOR_ABSTRACT: Name of the abstract unit for which reflection is created.
# - NAME: Name that should be used for new concrete unit that will store reflection.
# - GLOB: GLOB expressions that are used to gather headers from which reflection will be generated.
# - DIRECT: Direct list of headers from which reflection will be generated.
function (create_accompanying_reflection_unit)
    cmake_parse_arguments (ARG "" "FOR_ABSTRACT;NAME" "GLOB;DIRECT" ${ARGV})
    if (DEFINED ARG_UNPARSED_ARGUMENTS OR
            NOT DEFINED ARG_FOR_ABSTRACT OR
            NOT DEFINED ARG_NAME OR (
            NOT DEFINED ARG_GLOB AND
            NOT DEFINED ARG_DIRECT))
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    set (INPUTS)
    if (DEFINED ARG_GLOB)
        foreach (PATTERN ${ARG_GLOB})
            file (GLOB_RECURSE SOURCES "${PATTERN}")
            list (APPEND INPUTS ${SOURCES})
        endforeach ()
    endif ()

    if (DEFINED ARG_DIRECT)
        list (APPEND INPUTS ${ARG_DIRECT})
    endif ()

    set (GENERATED_FILE "${CMAKE_CURRENT_SOURCE_DIR}/reflection_includes.generated.c")
    set (CONTENT)

    foreach (INPUT ${INPUTS})
        string (APPEND CONTENT "#include \"${INPUT}\"\n")
    endforeach ()

    register_concrete ("${ARG_NAME}")
    file_write_if_not_equal ("${GENERATED_FILE}" "${CONTENT}")
    concrete_sources_direct ("${GENERATED_FILE}")

    concrete_implements_abstract ("${ARG_FOR_ABSTRACT}")
    concrete_require (SCOPE PRIVATE ABSTRACT error reflection)

    setup_core_preprocessing ()
    reflection_preprocessor_setup_step (DIRECT ${INPUTS} OVERRIDE_UNIT_NAME "${ARG_FOR_ABSTRACT}")
    concrete_preprocessing_queue_exclude_source (${GENERATED_FILE})
    register_unit_reflection_with_name ("${ARG_FOR_ABSTRACT}")
endfunction ()
