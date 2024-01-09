# Contains functions for integration with context unit.

# Path to context statics template file.
set (KAN_CONTEXT_STATICS_TEMPLATE "${CMAKE_SOURCE_DIR}/cmake/kan/context_statics.c")

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

        set (SYSTEM_APIS_LIST ${SYSTEMS})
        set (SYSTEM_APIS_DECLARATIONS ${SYSTEMS})

        list (TRANSFORM SYSTEM_APIS_LIST PREPEND "&KAN_CONTEXT_SYSTEM_API_NAME (")
        list (TRANSFORM SYSTEM_APIS_LIST APPEND ")")
        list (JOIN SYSTEM_APIS_LIST ",\n    " SYSTEM_APIS_LIST)

        list (TRANSFORM SYSTEM_APIS_DECLARATIONS PREPEND
                "IMPORT_THIS extern struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (")
        list (TRANSFORM SYSTEM_APIS_DECLARATIONS APPEND ")")
        list (JOIN SYSTEM_APIS_DECLARATIONS ";\n" SYSTEM_APIS_DECLARATIONS)

        set (CONTEXT_STATICS_FILE "${CMAKE_CURRENT_BINARY_DIR}/Generated/context_statics_${ARTEFACT_NAME}.c")
        message (STATUS "    Save context statics as \"${CONTEXT_STATICS_FILE}\".")
        configure_file ("${KAN_CONTEXT_STATICS_TEMPLATE}" "${CONTEXT_STATICS_FILE}")

        register_concrete ("${ARTEFACT_NAME}_context_statics")
        concrete_sources_direct ("${CONTEXT_STATICS_FILE}")
        concrete_require (SCOPE PRIVATE CONCRETE_INTERFACE context)
        shared_library_include (SCOPE PUBLIC CONCRETE "${ARTEFACT_NAME}_context_statics")
    endif ()
endfunction ()
