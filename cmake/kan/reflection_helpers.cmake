# Contains helper routines for setting up reflection generation.

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

    concrete_preprocessing_queue_step_preprocess ()
    reflection_preprocessor_setup_step (DIRECT ${INPUTS} OVERRIDE_UNIT_NAME "${ARG_FOR_ABSTRACT}")
    concrete_preprocessing_queue_exclude_source (${GENERATED_FILE})
    register_unit_reflection_with_name ("${ARG_FOR_ABSTRACT}")
endfunction ()
