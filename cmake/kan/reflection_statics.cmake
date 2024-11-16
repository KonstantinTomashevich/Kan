# Contains functions for informing context reflection system about static reflection registrars.

# Path to reflection statics ecosystem subdirectory.
# Reflection statics are split into ecosystems in order to put them into flattened binary directories.
set (KAN_REFLECTION_STATICS_ECOSYSTEM "${PROJECT_SOURCE_DIR}/cmake/kan/reflection_statics_ecosystem")

# Target property that holds list of reflection registrar names.
define_property (TARGET PROPERTY REFLECTION_REGISTRARS
        BRIEF_DOCS "Names of reflection registrars that are exported by this target."
        FULL_DOCS "This list is used to autogenerate reflection statics.")

# Informs build system that current unit exports reflection registrar with given name,
function (register_unit_reflection_with_name REGISTRAR_NAME)
    get_target_property (REFLECTION_REGISTRARS "${UNIT_NAME}" REFLECTION_REGISTRARS)
    if (REFLECTION_REGISTRARS STREQUAL "REFLECTION_REGISTRARS-NOTFOUND")
        set (REFLECTION_REGISTRARS)
    endif ()

    list (APPEND REFLECTION_REGISTRARS "${REGISTRAR_NAME}")
    set_target_properties ("${UNIT_NAME}" PROPERTIES REFLECTION_REGISTRARS "${REFLECTION_REGISTRARS}")
endfunction ()

# Informs build system that current unit exports reflection registrar with name equal to unit name.
function (register_unit_reflection)
    register_unit_reflection_with_name ("${UNIT_NAME}")
endfunction ()

# Generates reflection statics for current artefact by scanning all units visible from current artefact.
# Reflection statics are generated as separate single-file unit and are also included into resulting artefact.
# Arguments:
# - LOCAL_ONLY: optional flag argument. If passed, search for reflection registrars will be done in artefact scope.
function (generate_artefact_reflection_data)
    cmake_parse_arguments (OPTIONS "LOCAL_ONLY" "" "" ${ARGV})
    if (DEFINED OPTIONS_UNPARSED_ARGUMENTS)
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    set (REGISTRARS)
    if (DEFINED OPTIONS_LOCAL_ONLY)
        find_linked_targets_recursively (TARGET "${ARTEFACT_NAME}" OUTPUT ALL_VISIBLE_TARGETS ARTEFACT_SCOPE)
    else ()
        find_linked_targets_recursively (TARGET "${ARTEFACT_NAME}" OUTPUT ALL_VISIBLE_TARGETS CHECK_VISIBILITY)
    endif ()

    foreach (VISIBLE_TARGET ${ALL_VISIBLE_TARGETS})
        get_target_property (REFLECTION_REGISTRARS "${VISIBLE_TARGET}" REFLECTION_REGISTRARS)
        if (REFLECTION_REGISTRARS STREQUAL "REFLECTION_REGISTRARS-NOTFOUND")
            continue ()
        endif ()

        foreach (REGISTRAR ${REFLECTION_REGISTRARS})
            list (APPEND REGISTRARS "${REGISTRAR}")
        endforeach ()
    endforeach ()

    list (LENGTH REGISTRARS REGISTRARS_COUNT)
    if (REGISTRARS_COUNT GREATER 0)
        message (STATUS "    Generate reflection statics. Visible registrars:")
        foreach (REGISTRAR ${REGISTRARS})
            message (STATUS "        - \"${REGISTRAR}\"")
        endforeach ()

        set (REFLECTION_REGISTRARS_DECLARATIONS ${REGISTRARS})
        set (REFLECTION_REGISTRARS_CALLS ${REGISTRARS})

        list (TRANSFORM REFLECTION_REGISTRARS_DECLARATIONS PREPEND "KAN_REFLECTION_EXPECT_UNIT_REGISTRAR (")
        list (TRANSFORM REFLECTION_REGISTRARS_DECLARATIONS APPEND ")")
        list (JOIN REFLECTION_REGISTRARS_DECLARATIONS ";\n" REFLECTION_REGISTRARS_DECLARATIONS)

        list (TRANSFORM REFLECTION_REGISTRARS_CALLS PREPEND
                "KAN_REFLECTION_UNIT_REGISTRAR_NAME (")
        list (TRANSFORM REFLECTION_REGISTRARS_CALLS APPEND ") (registry)")
        list (JOIN REFLECTION_REGISTRARS_CALLS ";\n    " REFLECTION_REGISTRARS_CALLS)

        get_next_flattened_binary_directory (TEMP_DIRECTORY)
        add_subdirectory ("${KAN_REFLECTION_STATICS_ECOSYSTEM}" "${TEMP_DIRECTORY}")
    endif ()
endfunction ()
