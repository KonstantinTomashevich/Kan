# Contains functions for integration with context unit.

# Path to context statics ecosystem subdirectory.
# Context statics are split into ecosystems in order to put them into flattened binary directories.
set (KAN_CONTEXT_STATICS_ECOSYSTEM "${CMAKE_SOURCE_DIR}/cmake/kan/context_statics_ecosystem")

# Target property that holds list of attached systems.
define_property (TARGET PROPERTY CONTEXT_SYSTEMS
        BRIEF_DOCS "Names of context systems that are exported by this target."
        FULL_DOCS "This list is used to autogenerate context statics.")

# Informs build system that current unit exports system with given name to the context.
function (register_context_system SYSTEM_NAME)
    get_target_property (CONTEXT_SYSTEMS "${UNIT_NAME}" CONTEXT_SYSTEMS)
    if (CONTEXT_SYSTEMS STREQUAL "CONTEXT_SYSTEMS-NOTFOUND")
        set (CONTEXT_SYSTEMS)
    endif ()

    list (APPEND CONTEXT_SYSTEMS "${SYSTEM_NAME}")
    set_target_properties ("${UNIT_NAME}" PROPERTIES CONTEXT_SYSTEMS "${CONTEXT_SYSTEMS}")
endfunction ()

# Generates context statics for current artefact by scanning all units visible from current artefact.
# Context statics are generated as separate single-file unit and are also included into resulting artefact.
function (generate_artefact_context_data)
    set (SYSTEMS)
    find_linked_targets_recursively (TARGET "${ARTEFACT_NAME}" OUTPUT ALL_VISIBLE_TARGETS CHECK_VISIBILITY)

    foreach (VISIBLE_TARGET ${ALL_VISIBLE_TARGETS})
        get_target_property (CONTEXT_SYSTEMS "${VISIBLE_TARGET}" CONTEXT_SYSTEMS)
        if (CONTEXT_SYSTEMS STREQUAL "CONTEXT_SYSTEMS-NOTFOUND")
            continue ()
        endif ()

        foreach (SYSTEM ${CONTEXT_SYSTEMS})
            list (APPEND SYSTEMS "${SYSTEM}")
        endforeach ()
    endforeach ()

    list (LENGTH SYSTEMS SYSTEMS_COUNT)
    if (SYSTEMS_COUNT GREATER 0)
        message (STATUS "    Generate context statics. Visible systems:")
        foreach (SYSTEM ${SYSTEMS})
            message (STATUS "        - \"${SYSTEM}\"")
        endforeach ()

        set (SYSTEM_APIS_SETTERS_WITHOUT_INDEX ${SYSTEMS})
        set (SYSTEM_APIS_DECLARATIONS ${SYSTEMS})

        list (TRANSFORM SYSTEM_APIS_SETTERS_WITHOUT_INDEX PREPEND
                "KAN_CONTEXT_SYSTEM_ARRAY_NAME[SYSTEM_INDEX] = &KAN_CONTEXT_SYSTEM_API_NAME (")
        list (TRANSFORM SYSTEM_APIS_SETTERS_WITHOUT_INDEX APPEND ")")
        set (SYSTEM_APIS_SETTERS)
        set (SYSTEM_INDEX "0")

        foreach (SETTER ${SYSTEM_APIS_SETTERS_WITHOUT_INDEX})
            string (REPLACE "SYSTEM_INDEX" "${SYSTEM_INDEX}u" SETTER "${SETTER}")
            list (APPEND SYSTEM_APIS_SETTERS "${SETTER}")
            math (EXPR SYSTEM_INDEX "${SYSTEM_INDEX} + 1")
        endforeach ()

        list (JOIN SYSTEM_APIS_SETTERS ";\n    " SYSTEM_APIS_SETTERS)

        list (TRANSFORM SYSTEM_APIS_DECLARATIONS PREPEND
                "IMPORT_THIS extern struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (")
        list (TRANSFORM SYSTEM_APIS_DECLARATIONS APPEND ")")
        list (JOIN SYSTEM_APIS_DECLARATIONS ";\n" SYSTEM_APIS_DECLARATIONS)

        get_next_flattened_binary_directory (TEMP_DIRECTORY)
        add_subdirectory ("${KAN_CONTEXT_STATICS_ECOSYSTEM}" "${TEMP_DIRECTORY}")
    endif ()
endfunction ()
