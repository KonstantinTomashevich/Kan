# Contains functions for integration with context unit.

# Path to context statics ecosystem subdirectory.
# Context statics are split into ecosystems in order to put them into flattened binary directories.
set (KAN_CONTEXT_STATICS_ECOSYSTEM "${PROJECT_SOURCE_DIR}/cmake/kan/context_statics_ecosystem")

# Directory with generated files that should be added to context unit includes.
set (KAN_CONTEXT_ALL_SYSTEM_NAMES_INCLUDE_BASE "${CMAKE_BINARY_DIR}/Generated/context/all_systems")

# Path at which include with all system names will be generated.
set (KAN_CONTEXT_ALL_SYSTEM_NAMES_FILE "${KAN_CONTEXT_ALL_SYSTEM_NAMES_INCLUDE_BASE}/kan/context/all_system_names.h")

# Target property that holds list of attached systems.
define_property (TARGET PROPERTY CONTEXT_SYSTEMS
        BRIEF_DOCS "Names of context systems that are exported by this target."
        FULL_DOCS "This list is used to autogenerate context statics.")

# Custom target for storing property that contains content for all system names header.
add_custom_target (context_generation_properties)

# Informs build system that current unit exports system with given name to the context.
# Arguments:
# - NAME: System name for the registration.
# - PRIVATE: Flag that excludes system from all system names header generation when passed.
function (register_context_system)
    cmake_parse_arguments (ARG "PRIVATE" "NAME" "" ${ARGV})
    if (DEFINED ARG_UNPARSED_ARGUMENTS OR NOT DEFINED ARG_NAME)
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    get_target_property (CONTEXT_SYSTEMS "${UNIT_NAME}" CONTEXT_SYSTEMS)
    if (CONTEXT_SYSTEMS STREQUAL "CONTEXT_SYSTEMS-NOTFOUND")
        set (CONTEXT_SYSTEMS)
    endif ()

    list (APPEND CONTEXT_SYSTEMS "${ARG_NAME}")
    set_target_properties ("${UNIT_NAME}" PROPERTIES CONTEXT_SYSTEMS "${CONTEXT_SYSTEMS}")

    if (NOT ARG_PRIVATE)
        get_target_property (CONTENT context_generation_properties INTERNAL_CONTEXT_ALL_SYSTEM_NAMES_CONTENT)
        if (NOT CONTENT)
            set (CONTENT "#pragma once\n")
            file (GENERATE OUTPUT "${KAN_CONTEXT_ALL_SYSTEM_NAMES_FILE}"
                    CONTENT
                    "$<TARGET_PROPERTY:context_generation_properties,INTERNAL_CONTEXT_ALL_SYSTEM_NAMES_CONTENT>")
        endif ()

        # Ensure that name is not already added, otherwise we might end up with duplicate macros.
        get_target_property (NAMES context_generation_properties INTERNAL_CONTEXT_ALL_SYSTEM_NAMES)

        if (NOT NAMES)
            set (NAMES)
        endif ()
        
        list (FIND NAMES "${ARG_NAME}" FOUND_INDEX)
        if (NOT FOUND_INDEX EQUAL -1)
            return ()
        endif ()
        
        list (APPEND NAMES "${ARG_NAME}")
        string (REGEX REPLACE "_t$" "" SYSTEM_NAME_DEFINE "${ARG_NAME}")
        string (TOUPPER "${SYSTEM_NAME_DEFINE}" SYSTEM_NAME_DEFINE)
        string (APPEND CONTENT "#define KAN_CONTEXT_${SYSTEM_NAME_DEFINE}_NAME \"${ARG_NAME}\"\n")

        set_target_properties (context_generation_properties PROPERTIES
                INTERNAL_CONTEXT_ALL_SYSTEM_NAMES_CONTENT "${CONTENT}"
                INTERNAL_CONTEXT_ALL_SYSTEM_NAMES "${NAMES}")
    endif ()
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
