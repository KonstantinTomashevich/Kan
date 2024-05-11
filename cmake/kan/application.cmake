# Contains pipeline for creating application framework applications.

# Name of the directory to store configurations.
set (KAN_APPLICATION_CONFIGURATION_DIRECTORY_NAME "configuration")

# Name of the directory to store plugins.
set (KAN_APPLICATION_PLUGINS_DIRECTORY_NAME "plugins")

# Name of the directory to store resources.
set (KAN_APPLICATION_RESOURCES_DIRECTORY_NAME "resources")

# Path to static data template for application framework static launcher.
set (KAN_APPLICATION_PROGRAM_LAUNCHER_STATICS_TEMPLATE "${CMAKE_SOURCE_DIR}/cmake/kan/application_launcher_statics.c")

# Path to static data template for application framework tool.
set (KAN_APPLICATION_TOOL_STATICS_TEMPLATE "${CMAKE_SOURCE_DIR}/cmake/kan/application_tool_statics.c")

# Name of the used application framework static launcher implementation.
set (KAN_APPLICATION_PROGRAM_LAUNCHER_IMPLEMENTATION "sdl")

# Target properties, used to store application framework related data. Shouldn't be directly modified by user.

define_property (TARGET PROPERTY APPLICATION_CORE_ABSTRACT
        BRIEF_DOCS "Contains list of abstract implementations included by application core library."
        FULL_DOCS "Contains list of abstract implementations included by application core library.")

define_property (TARGET PROPERTY APPLICATION_CORE_CONCRETE
        BRIEF_DOCS "Contains list of concrete units included by application core library."
        FULL_DOCS "Contains list of concrete units included by application core library.")

define_property (TARGET PROPERTY APPLICATION_CORE_CONFIGURATION
        BRIEF_DOCS "Contains path to application core configuration source file."
        FULL_DOCS "Configuration source file is configured with automatically provided plugins and directories.")

define_property (TARGET PROPERTY APPLICATION_CORE_PLUGIN_GROUPS
        BRIEF_DOCS "Contains list of plugin groups used as core plugins."
        FULL_DOCS "Contains list of plugin groups used as core plugins.")

define_property (TARGET PROPERTY APPLICATION_WORLD_DIRECTORY
        BRIEF_DOCS "Contains path to application world directory folder."
        FULL_DOCS "Contains path to application world directory folder.")

define_property (TARGET PROPERTY APPLICATION_DEVELOPMENT_ENVIRONMENT_TAGS
        BRIEF_DOCS "Contains universe environment tags passed to development core configuration."
        FULL_DOCS "Contains universe environment tags passed to development core configuration.")

define_property (TARGET PROPERTY APPLICATION_PLUGINS
        BRIEF_DOCS "Contains list of internal plugin targets."
        FULL_DOCS "Contains list of internal plugin targets.")

define_property (TARGET PROPERTY APPLICATION_PLUGIN_NAME
        BRIEF_DOCS "Contains plugin readable name."
        FULL_DOCS "Contains plugin readable name.")

define_property (TARGET PROPERTY APPLICATION_PLUGIN_GROUP
        BRIEF_DOCS "Contains plugin group name."
        FULL_DOCS "Contains plugin group name.")

define_property (TARGET PROPERTY APPLICATION_PLUGIN_ABSTRACT
        BRIEF_DOCS "Contains list of abstract implementations included by plugin library."
        FULL_DOCS "Contains list of abstract implementations included by plugin library.")

define_property (TARGET PROPERTY APPLICATION_PLUGIN_CONCRETE
        BRIEF_DOCS "Contains list of concrete units included by plugin library."
        FULL_DOCS "Contains list of concrete units included by plugin library.")

define_property (TARGET PROPERTY APPLICATION_PROGRAMS
        BRIEF_DOCS "Contains list of internal program targets."
        FULL_DOCS "Contains list of internal program targets.")

define_property (TARGET PROPERTY APPLICATION_PROGRAM_NAME
        BRIEF_DOCS "Contains program readable name."
        FULL_DOCS "Contains program readable name.")

define_property (TARGET PROPERTY APPLICATION_PROGRAM_CONFIGURATION
        BRIEF_DOCS "Contains path to program configuration source file."
        FULL_DOCS "Configuration source file is configured with automatically provided plugins and directories.")

define_property (TARGET PROPERTY APPLICATION_PROGRAM_PLUGIN_GROUPS
        BRIEF_DOCS "Contains list of plugin groups used as program plugins."
        FULL_DOCS "Contains list of plugin groups used as program plugins.")

define_property (TARGET PROPERTY UNIT_RESOURCE_TARGETS
        BRIEF_DOCS "List of targets that contain information about resources used by this unit."
        FULL_DOCS "List of targets that contain information about resources used by this unit.")

define_property (TARGET PROPERTY UNIT_RESOURCE_TARGET_TYPE
        BRIEF_DOCS "Type of resource target. Either USUAL, CUSTOM or PRE_MADE."
        FULL_DOCS "Type of resource target. Either USUAL, CUSTOM or PRE_MADE.")

define_property (TARGET PROPERTY UNIT_RESOURCE_TARGET_SOURCE_DIRECTORY
        BRIEF_DOCS "Contains absolute path to directory with source resource files."
        FULL_DOCS "Contains absolute path to directory with source resource files.")

# Starts application configuration registration routine.
function (register_application NAME)
    set (APPLICATION_NAME "${NAME}" PARENT_SCOPE)
    add_custom_target ("${NAME}" ALL)
    message (STATUS "Registering application \"${NAME}\".")
endfunction ()

# Includes given abstract implementations and concrete units to application core library.
# Arguments:
# - ABSTRACT: list of included abstract units. Follows shared_library_include format.
# - CONCRETE: list of included concrete units.
function (application_core_include)
    cmake_parse_arguments (INCLUDE "" "" "ABSTRACT;CONCRETE" ${ARGV})
    if (DEFINED INCLUDE_UNPARSED_ARGUMENTS OR (
            NOT DEFINED INCLUDE_ABSTRACT AND
            NOT DEFINED INCLUDE_CONCRETE))
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    foreach (ABSTRACT ${INCLUDE_ABSTRACT})
        message (STATUS "    Include abstract implementation unit to application core \"${ABSTRACT}\".")
    endforeach ()

    foreach (CONCRETE ${INCLUDE_CONCRETE})
        message (STATUS "    Include concrete unit to application core \"${CONCRETE}\".")
    endforeach ()

    get_target_property (CORE_ABSTRACT "${APPLICATION_NAME}" APPLICATION_CORE_ABSTRACT)
    if (CORE_ABSTRACT STREQUAL "CORE_ABSTRACT-NOTFOUND")
        set (CORE_ABSTRACT)
    endif ()

    get_target_property (CORE_CONCRETE "${APPLICATION_NAME}" APPLICATION_CORE_CONCRETE)
    if (CORE_CONCRETE STREQUAL "CORE_CONCRETE-NOTFOUND")
        set (CORE_CONCRETE)
    endif ()

    list (APPEND CORE_ABSTRACT "${INCLUDE_ABSTRACT}")
    list (APPEND CORE_CONCRETE "${INCLUDE_CONCRETE}")

    set_target_properties ("${APPLICATION_NAME}" PROPERTIES
            APPLICATION_CORE_ABSTRACT "${CORE_ABSTRACT}"
            APPLICATION_CORE_CONCRETE "${CORE_CONCRETE}")
endfunction ()

# Sets path to application core configuration source file.
function (application_core_set_configuration CONFIGURATION)
    cmake_path (ABSOLUTE_PATH CONFIGURATION NORMALIZE)
    set_target_properties ("${APPLICATION_NAME}" PROPERTIES APPLICATION_CORE_CONFIGURATION "${CONFIGURATION}")
    message (STATUS "    Setting core configuration to \"${CONFIGURATION}\".")
endfunction ()

# Sets path to application core world directory.
function (application_set_world_directory DIRECTORY)
    cmake_path (ABSOLUTE_PATH DIRECTORY NORMALIZE)
    set_target_properties ("${APPLICATION_NAME}" PROPERTIES APPLICATION_WORLD_DIRECTORY "${DIRECTORY}")
    message (STATUS "    Setting core world directory to \"${DIRECTORY}\".")
endfunction ()

# Sets path to application core world directory.
function (application_add_development_environment_tag TAG)
    message (STATUS "    Adding development environment tag \"${TAG}\".")
    get_target_property (TAGS "${APPLICATION_NAME}" APPLICATION_DEVELOPMENT_ENVIRONMENT_TAGS)

    if (TAGS STREQUAL "TAGS-NOTFOUND")
        set (TAGS)
    endif ()

    list (APPEND TAGS "${TAG}")
    set_target_properties ("${APPLICATION_NAME}" PROPERTIES APPLICATION_DEVELOPMENT_ENVIRONMENT_TAGS "${TAGS}")
endfunction ()

# Adds given plugin group to core plugins list.
function (application_core_use_plugin_group GROUP)
    get_target_property (PLUGIN_GROUPS "${APPLICATION_NAME}" APPLICATION_CORE_PLUGIN_GROUPS)
    if (PLUGIN_GROUPS STREQUAL "PLUGIN_GROUPS-NOTFOUND")
        set (PLUGIN_GROUPS)
    endif ()

    message (STATUS "    Use plugin group \"${GROUP}\" in application core.")
    list (APPEND PLUGIN_GROUPS "${GROUP}")
    set_target_properties ("${APPLICATION_NAME}" PROPERTIES APPLICATION_CORE_PLUGIN_GROUPS "${PLUGIN_GROUPS}")
endfunction ()

# Starts application plugin registration routine. Must be called inside application registration routine.
# Arguments:
# - NAME: Plugin readable name.
# - GROUP: Name of the group to which plugin belongs.
function (register_application_plugin)
    cmake_parse_arguments (PLUGIN "" "NAME;GROUP" "" ${ARGV})
    if (DEFINED PLUGIN_UNPARSED_ARGUMENTS OR
            NOT DEFINED PLUGIN_NAME OR
            NOT DEFINED PLUGIN_GROUP)
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    message (STATUS "    Registering plugin \"${PLUGIN_NAME}\" in group \"${PLUGIN_GROUP}\".")
    set (APPLICATION_PLUGIN_NAME "${PLUGIN_NAME}" PARENT_SCOPE)
    add_custom_target ("${APPLICATION_NAME}_plugin_${PLUGIN_NAME}" ALL)
    set_target_properties ("${APPLICATION_NAME}_plugin_${PLUGIN_NAME}" PROPERTIES
            APPLICATION_PLUGIN_NAME "${PLUGIN_NAME}"
            APPLICATION_PLUGIN_GROUP "${PLUGIN_GROUP}")

    add_dependencies ("${APPLICATION_NAME}" "${APPLICATION_NAME}_plugin_${PLUGIN_NAME}")
    get_target_property (PLUGINS "${APPLICATION_NAME}" APPLICATION_PLUGINS)

    if (PLUGINS STREQUAL "PLUGINS-NOTFOUND")
        set (PLUGINS)
    endif ()

    list (APPEND PLUGINS "${APPLICATION_NAME}_plugin_${PLUGIN_NAME}")
    set_target_properties ("${APPLICATION_NAME}" PROPERTIES APPLICATION_PLUGINS "${PLUGINS}")
endfunction ()

# Includes given abstract implementations and concrete units to current plugin library.
# Arguments:
# - ABSTRACT: list of included abstract units. Follows shared_library_include format.
# - CONCRETE: list of included concrete units.
function (application_plugin_include)
    cmake_parse_arguments (INCLUDE "" "" "ABSTRACT;CONCRETE" ${ARGV})
    if (DEFINED INCLUDE_UNPARSED_ARGUMENTS OR (
            NOT DEFINED INCLUDE_ABSTRACT AND
            NOT DEFINED INCLUDE_CONCRETE))
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    foreach (ABSTRACT ${INCLUDE_ABSTRACT})
        message (STATUS "        Include abstract implementation unit \"${ABSTRACT}\".")
    endforeach ()

    foreach (CONCRETE ${INCLUDE_CONCRETE})
        message (STATUS "        Include concrete unit \"${CONCRETE}\".")
    endforeach ()

    get_target_property (
            PLUGIN_ABSTRACT "${APPLICATION_NAME}_plugin_${APPLICATION_PLUGIN_NAME}" APPLICATION_PLUGIN_ABSTRACT)

    if (PLUGIN_ABSTRACT STREQUAL "PLUGIN_ABSTRACT-NOTFOUND")
        set (PLUGIN_ABSTRACT)
    endif ()

    get_target_property (
            PLUGIN_CONCRETE "${APPLICATION_NAME}_plugin_${APPLICATION_PLUGIN_NAME}" APPLICATION_PLUGIN_CONCRETE)

    if (PLUGIN_CONCRETE STREQUAL "PLUGIN_CONCRETE-NOTFOUND")
        set (PLUGIN_CONCRETE)
    endif ()

    list (APPEND PLUGIN_ABSTRACT "${INCLUDE_ABSTRACT}")
    list (APPEND PLUGIN_CONCRETE "${INCLUDE_CONCRETE}")

    set_target_properties ("${APPLICATION_NAME}_plugin_${APPLICATION_PLUGIN_NAME}" PROPERTIES
            APPLICATION_PLUGIN_ABSTRACT "${PLUGIN_ABSTRACT}"
            APPLICATION_PLUGIN_CONCRETE "${PLUGIN_CONCRETE}")
endfunction ()

# Starts application program registration routine. Must be called inside application registration routine.
function (register_application_program NAME)
    message (STATUS "    Registering program \"${NAME}\".")
    set (APPLICATION_PROGRAM_NAME "${NAME}" PARENT_SCOPE)
    add_custom_target ("${APPLICATION_NAME}_program_${NAME}" ALL)
    set_target_properties ("${APPLICATION_NAME}_program_${NAME}" PROPERTIES APPLICATION_PROGRAM_NAME "${NAME}")

    add_dependencies ("${APPLICATION_NAME}" "${APPLICATION_NAME}_program_${NAME}")
    get_target_property (PROGRAMS "${APPLICATION_NAME}" APPLICATION_PROGRAMS)

    if (PROGRAMS STREQUAL "PROGRAMS-NOTFOUND")
        set (PROGRAMS)
    endif ()

    list (APPEND PROGRAMS "${APPLICATION_NAME}_program_${NAME}")
    set_target_properties ("${APPLICATION_NAME}" PROPERTIES APPLICATION_PROGRAMS "${PROGRAMS}")
endfunction ()

# Sets path to current program configuration source file.
function (application_program_set_configuration CONFIGURATION)
    cmake_path (ABSOLUTE_PATH CONFIGURATION NORMALIZE)
    message (STATUS "        Setting configuration to \"${CONFIGURATION}\".")
    set_target_properties ("${APPLICATION_NAME}_program_${APPLICATION_PROGRAM_NAME}" PROPERTIES
            APPLICATION_PROGRAM_CONFIGURATION "${CONFIGURATION}")
endfunction ()

# Adds given plugin group to current program plugins list.
function (application_program_use_plugin_group GROUP)
    get_target_property (PLUGIN_GROUPS "${APPLICATION_NAME}_program_${APPLICATION_PROGRAM_NAME}"
            APPLICATION_PROGRAM_PLUGIN_GROUPS)

    if (PLUGIN_GROUPS STREQUAL "PLUGIN_GROUPS-NOTFOUND")
        set (PLUGIN_GROUPS)
    endif ()

    message (STATUS "        Use plugin group \"${GROUP}\".")
    list (APPEND PLUGIN_GROUPS "${GROUP}")
    set_target_properties ("${APPLICATION_NAME}_program_${APPLICATION_PROGRAM_NAME}" PROPERTIES
            APPLICATION_PROGRAM_PLUGIN_GROUPS "${PLUGIN_GROUPS}")
endfunction ()

# Uses data gathered by registration functions above to generate application shared libraries, executables and other
# application related targets.
function (application_generate)
    message (STATUS "Application \"${APPLICATION_NAME}\" registration done, generating...")

    # Find core plugins, we'll need them later.

    set (CORE_PLUGINS)
    get_target_property (PLUGINS "${APPLICATION_NAME}" APPLICATION_PLUGINS)

    if (PLUGINS STREQUAL "PLUGINS-NOTFOUND")
        set (PLUGINS)
    endif ()

    get_target_property (CORE_GROUPS "${APPLICATION_NAME}" APPLICATION_CORE_PLUGIN_GROUPS)
    if (NOT CORE_GROUPS STREQUAL "CORE_GROUPS-NOTFOUND")
        foreach (PLUGIN ${PLUGINS})
            get_target_property (PLUGIN_GROUP "${PLUGIN}" APPLICATION_PLUGIN_GROUP)
            if ("${PLUGIN_GROUP}" IN_LIST CORE_GROUPS)
                list (APPEND CORE_PLUGINS "${PLUGIN}")
            endif ()
        endforeach ()
    endif ()

    # The routine below generates build with separate plugin libraries, separate executables and core library.
    # It is good for development (as it allows plugin hot reload) and it is good for desktop platforms.
    # But we'll need merged builds for mobile platforms later.

    # Register core library first.

    register_shared_library ("${APPLICATION_NAME}_core_library")
    get_target_property (APPLICATION_ABSTRACT "${APPLICATION_NAME}" APPLICATION_CORE_ABSTRACT)

    if (NOT APPLICATION_ABSTRACT STREQUAL "APPLICATION_ABSTRACT-NOTFOUND" AND APPLICATION_ABSTRACT)
        shared_library_include (SCOPE PUBLIC ABSTRACT ${APPLICATION_ABSTRACT})
    endif ()

    get_target_property (APPLICATION_CONCRETE "${APPLICATION_NAME}" APPLICATION_CORE_CONCRETE)
    if (NOT APPLICATION_CONCRETE STREQUAL "APPLICATION_CONCRETE-NOTFOUND" AND APPLICATION_CONCRETE)
        shared_library_include (SCOPE PUBLIC CONCRETE ${APPLICATION_CONCRETE})
    endif ()

    generate_artefact_context_data ()
    generate_artefact_reflection_data ()
    shared_library_verify ()
    shared_library_copy_linked_artefacts ()

    # Set development directories.
    set (DEV_BUILD_DIRECTORY "$<TARGET_FILE_DIR:${APPLICATION_NAME}_core_library>")
    set (DEV_CONFIGURATION_DIRECTORY "${DEV_BUILD_DIRECTORY}/${KAN_APPLICATION_CONFIGURATION_DIRECTORY_NAME}")
    set (DEV_PLUGINS_DIRECTORY "${DEV_BUILD_DIRECTORY}/${KAN_APPLICATION_PLUGINS_DIRECTORY_NAME}")
    set (DEV_RESOURCES_DIRECTORY "${DEV_BUILD_DIRECTORY}/${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}")

    add_custom_target ("${APPLICATION_NAME}_prepare_dev_directories" ALL
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEV_CONFIGURATION_DIRECTORY}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEV_PLUGINS_DIRECTORY}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEV_RESOURCES_DIRECTORY}"
            COMMENT "Creating development build directories for application \"${APPLICATION_NAME}\".")

    # Generate core plugin libraries first, because other plugins depend on core plugins.

    foreach (PLUGIN ${CORE_PLUGINS})
        register_shared_library ("${PLUGIN}_library")
        get_target_property (PLUGIN_ABSTRACT "${PLUGIN}" APPLICATION_PLUGIN_ABSTRACT)

        if (NOT PLUGIN_ABSTRACT STREQUAL "PLUGIN_ABSTRACT-NOTFOUND" AND PLUGIN_ABSTRACT)
            shared_library_include (SCOPE PUBLIC ABSTRACT ${PLUGIN_ABSTRACT})
        endif ()

        get_target_property (PLUGIN_CONCRETE "${PLUGIN}" APPLICATION_PLUGIN_CONCRETE)
        if (NOT PLUGIN_CONCRETE STREQUAL "PLUGIN_CONCRETE-NOTFOUND" AND PLUGIN_CONCRETE)
            shared_library_include (SCOPE PUBLIC CONCRETE ${PLUGIN_CONCRETE})
        endif ()

        shared_library_link_shared_library ("${PLUGIN}_library" PUBLIC "${APPLICATION_NAME}_core_library")
        generate_artefact_reflection_data (LOCAL_ONLY)
        shared_library_verify ()
    endforeach ()

    # Generate non-core plugins. These plugins link to core plugins as they are allowed to depend on them.

    foreach (PLUGIN ${PLUGINS})
        if (NOT PLUGIN IN_LIST CORE_PLUGINS)
            register_shared_library ("${PLUGIN}_library")
            get_target_property (PLUGIN_ABSTRACT "${PLUGIN}" APPLICATION_PLUGIN_ABSTRACT)

            if (NOT PLUGIN_ABSTRACT STREQUAL "PLUGIN_ABSTRACT-NOTFOUND" AND PLUGIN_ABSTRACT)
                shared_library_include (SCOPE PUBLIC ABSTRACT ${PLUGIN_ABSTRACT})
            endif ()

            get_target_property (PLUGIN_CONCRETE "${PLUGIN}" APPLICATION_PLUGIN_CONCRETE)
            if (NOT PLUGIN_CONCRETE STREQUAL "PLUGIN_CONCRETE-NOTFOUND" AND PLUGIN_CONCRETE)
                shared_library_include (SCOPE PUBLIC CONCRETE ${PLUGIN_CONCRETE})
            endif ()

            shared_library_link_shared_library ("${PLUGIN}_library" PUBLIC "${APPLICATION_NAME}_core_library")
            foreach (CORE_PLUGIN ${CORE_PLUGINS})
                shared_library_link_shared_library ("${PLUGIN}_library" PUBLIC "${CORE_PLUGIN}_library")
            endforeach ()

            generate_artefact_reflection_data (LOCAL_ONLY)
            shared_library_verify ()
        endif ()
    endforeach ()

    # Generate plugin library copy targets.

    foreach (PLUGIN ${PLUGINS})
        add_custom_target ("${PLUGIN}_dev_copy")
        setup_shared_library_copy (
                LIBRARY "${PLUGIN}_library"
                USER "${PLUGIN}_dev_copy"
                OUTPUT ${DEV_PLUGINS_DIRECTORY}
                DEPENDENCIES "${APPLICATION_NAME}_prepare_dev_directories")
    endforeach ()

    # Find core resource targets.

    set (CORE_RESOURCE_TARGETS)

    find_linked_targets_recursively (TARGET "${APPLICATION_NAME}_core_library" OUTPUT CORE_LINKED_TARGETS)
    foreach (LINKED_TARGET ${CORE_LINKED_TARGETS})
        get_target_property (THIS_RESOURCE_TARGETS "${LINKED_TARGET}" UNIT_RESOURCE_TARGETS)
        if (NOT THIS_RESOURCE_TARGETS STREQUAL "THIS_RESOURCE_TARGETS-NOTFOUND")
            list (APPEND CORE_RESOURCE_TARGETS ${THIS_RESOURCE_TARGETS})
        endif ()
    endforeach ()

    foreach (PLUGIN ${CORE_PLUGINS})
        find_linked_targets_recursively (TARGET "${PLUGIN}_library" OUTPUT PLUGIN_PLUGIN_TARGETS ARTEFACT_SCOPE)
        foreach (PLUGIN_TARGET ${PLUGIN_PLUGIN_TARGETS})
            get_target_property (THIS_RESOURCE_TARGETS "${PLUGIN_TARGET}" UNIT_RESOURCE_TARGETS)
            if (NOT THIS_RESOURCE_TARGETS STREQUAL "THIS_RESOURCE_TARGETS-NOTFOUND")
                list (APPEND CORE_RESOURCE_TARGETS ${THIS_RESOURCE_TARGETS})
            endif ()
        endforeach ()
    endforeach ()

    list (REMOVE_DUPLICATES CORE_RESOURCE_TARGETS)

    # Generate development core configuration.

    get_target_property (CORE_CONFIGURATION "${APPLICATION_NAME}" APPLICATION_CORE_CONFIGURATION)
    if (CORE_CONFIGURATION STREQUAL "CORE_CONFIGURATION-NOTFOUND")
        message (FATAL_ERROR "There is no core configuration for application \"${APPLICATION_NAME}\"!")
    endif ()

    get_target_property (CORE_WORLD_DIRECTORY "${APPLICATION_NAME}" APPLICATION_WORLD_DIRECTORY)
    if (CORE_WORLD_DIRECTORY STREQUAL "CORE_WORLD_DIRECTORY-NOTFOUND")
        message (FATAL_ERROR "There is no core world directory for application \"${APPLICATION_NAME}\"!")
    endif ()

    get_target_property (DEVELOPMENT_TAGS "${APPLICATION_NAME}" APPLICATION_DEVELOPMENT_ENVIRONMENT_TAGS)
    if (DEVELOPMENT_TAGS STREQUAL "DEVELOPMENT_TAGS-NOTFOUND")
        set (DEVELOPMENT_TAGS)
    endif ()

    set (DEV_CORE_CONFIGURATOR_CONTENT)
    foreach (PLUGIN ${CORE_PLUGINS})
        string (APPEND DEV_CORE_CONFIGURATOR_CONTENT "list (APPEND PLUGINS_LIST \"\\\"${PLUGIN}_library\\\"\")\n")
    endforeach ()

    string (APPEND DEV_CORE_CONFIGURATOR_CONTENT "list (JOIN PLUGINS_LIST \", \" PLUGINS)\n")
    foreach (RESOURCE_TARGET ${CORE_RESOURCE_TARGETS})
        get_target_property (SOURCE_DIRECTORY "${RESOURCE_TARGET}" UNIT_RESOURCE_TARGET_SOURCE_DIRECTORY)
        string (APPEND DEV_CORE_CONFIGURATOR_CONTENT "string (APPEND RESOURCE_DIRECTORIES \"+resource_directories ")
        string (APPEND DEV_CORE_CONFIGURATOR_CONTENT "{ path = \\\"${SOURCE_DIRECTORY}\\\" mount_path = ")
        string (APPEND DEV_CORE_CONFIGURATOR_CONTENT
                "\\\"${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}/${RESOURCE_TARGET}\\\" }\\n\")\n")
    endforeach ()

    foreach (DEVELOPMENT_TAG ${DEVELOPMENT_TAGS})
        string (APPEND DEV_CORE_CONFIGURATOR_CONTENT
                "list (APPEND ENVIRONMENT_TAGS_LIST \"\\\"${DEVELOPMENT_TAG}\\\"\")\n")
    endforeach ()

    string (APPEND DEV_CORE_CONFIGURATOR_CONTENT "list (JOIN ENVIRONMENT_TAGS_LIST \", \" ENVIRONMENT_TAGS)\n")
    string (APPEND DEV_CORE_CONFIGURATOR_CONTENT
            "set (PLUGINS_DIRECTORY_PATH \"${KAN_APPLICATION_PLUGINS_DIRECTORY_NAME}\")\n")
    string (APPEND DEV_CORE_CONFIGURATOR_CONTENT
            "set (WORLDS_DIRECTORY_PATH \"${CORE_WORLD_DIRECTORY}\")\n")
    string (APPEND DEV_CORE_CONFIGURATOR_CONTENT "set (OBSERVE_WORLD_DEFINITIONS 1)\n")

    set (DEV_CORE_CONFIGURATION_PATH "${DEV_CONFIGURATION_DIRECTORY}/core.rd")
    string (APPEND DEV_CORE_CONFIGURATOR_CONTENT
            "configure_file (\"${CORE_CONFIGURATION}\" \"${DEV_CORE_CONFIGURATION_PATH}\")")

    set (DEV_CORE_CONFIGURATOR_PATH "${DEV_BUILD_DIRECTORY}/${APPLICATION_NAME}_dev_core_config.cmake")
    file (GENERATE OUTPUT "${DEV_CORE_CONFIGURATOR_PATH}" CONTENT "${DEV_CORE_CONFIGURATOR_CONTENT}")

    add_custom_target ("${APPLICATION_NAME}_dev_core_configuration" ALL
            DEPENDS "${APPLICATION_NAME}_prepare_dev_directories"
            COMMAND "${CMAKE_COMMAND}" -P "${DEV_CORE_CONFIGURATOR_PATH}"
            COMMENT "Building core configuration for application \"${APPLICATION_NAME}\".")

    # Generate tool statics file.

    set (STATICS_PLUGINS_DIRECTORY_PATH "\"${KAN_APPLICATION_PLUGINS_DIRECTORY_NAME}\"")
    set (STATICS_PLUGINS)

    foreach (PLUGIN ${PLUGINS})
        string (APPEND STATICS_PLUGINS "    \"${PLUGIN}_library\",\n")
        if ("${PLUGIN_GROUP}" IN_LIST CORE_GROUPS)
            list (APPEND CORE_PLUGINS "${PLUGIN}")
        endif ()
    endforeach ()

    set (STATICS_PATH "${CMAKE_CURRENT_BINARY_DIR}/Generated/${APPLICATION_NAME}_tool_statics.c")
    configure_file ("${KAN_APPLICATION_TOOL_STATICS_TEMPLATE}" "${STATICS_PATH}")

    register_concrete ("${APPLICATION_NAME}_tool_statics")
    concrete_sources_direct ("${STATICS_PATH}")

    # Generate resource binarizer executable.

    register_executable ("${APPLICATION_NAME}_resource_binarizer")
    executable_include (
            CONCRETE
            application_framework_resource_binarizer application_framework_tool "${APPLICATION_NAME}_tool_statics")

    executable_link_shared_libraries ("${APPLICATION_NAME}_core_library")
    executable_verify ()
    executable_copy_linked_artefacts ()

    foreach (PLUGIN ${PLUGINS})
        add_dependencies ("${APPLICATION_NAME}_resource_binarizer" "${PLUGIN}_dev_copy")
    endforeach ()

    # Generate packer executable.

    register_executable ("${APPLICATION_NAME}_packer")
    executable_include (
            CONCRETE application_framework_packer application_framework_tool "${APPLICATION_NAME}_tool_statics")

    executable_link_shared_libraries ("${APPLICATION_NAME}_core_library")
    executable_verify ()
    executable_copy_linked_artefacts ()

    foreach (PLUGIN ${PLUGINS})
        add_dependencies ("${APPLICATION_NAME}_packer" "${PLUGIN}_dev_copy")
    endforeach ()

    # Generate programs.

    get_target_property (PROGRAMS "${APPLICATION_NAME}" APPLICATION_PROGRAMS)
    if (PROGRAMS STREQUAL "PROGRAMS-NOTFOUND")
        message (FATAL_ERROR "There is no programs for application \"${APPLICATION_NAME}\"!")
    endif ()

    foreach (PROGRAM ${PROGRAMS})
        get_target_property (PROGRAM_NAME "${PROGRAM}" APPLICATION_PROGRAM_NAME)

        # Find program-specific plugins, we'll need them later.

        set (PROGRAM_PLUGINS)
        get_target_property (PROGRAM_GROUPS "${PROGRAM}" APPLICATION_PROGRAM_PLUGIN_GROUPS)

        if (NOT PROGRAM_GROUPS STREQUAL "PROGRAM_GROUPS-NOTFOUND")
            foreach (PLUGIN ${PLUGINS})
                get_target_property (PLUGIN_GROUP "${PLUGIN}" APPLICATION_PLUGIN_GROUP)
                if ("${PLUGIN_GROUP}" IN_LIST PROGRAM_GROUPS)
                    list (APPEND PROGRAM_PLUGINS "${PLUGIN}")
                endif ()
            endforeach ()
        endif ()

        # Generate program executable.

        set (STATICS_CORE_CONFIGURATION_PATH "${KAN_APPLICATION_CONFIGURATION_DIRECTORY_NAME}/core.rd")
        set (STATICS_PROGRAM_CONFIGURATION_PATH
                "${KAN_APPLICATION_CONFIGURATION_DIRECTORY_NAME}/program_${PROGRAM_NAME}.rd")
        set (STATICS_PATH
                "${CMAKE_CURRENT_BINARY_DIR}/Generated/${APPLICATION_NAME}_program_${PROGRAM_NAME}_statics.c")
        configure_file ("${KAN_APPLICATION_PROGRAM_LAUNCHER_STATICS_TEMPLATE}" "${STATICS_PATH}")

        register_concrete ("${PROGRAM}_launcher_statics")
        concrete_sources_direct ("${STATICS_PATH}")

        register_executable ("${PROGRAM}_launcher")
        executable_include (
                ABSTRACT application_framework_static_launcher=${KAN_APPLICATION_PROGRAM_LAUNCHER_IMPLEMENTATION}
                CONCRETE "${PROGRAM}_launcher_statics")

        executable_link_shared_libraries ("${APPLICATION_NAME}_core_library")
        executable_verify ()
        executable_copy_linked_artefacts ()

        foreach (PLUGIN ${CORE_PLUGINS})
            add_dependencies ("${PROGRAM}_launcher" "${PLUGIN}_dev_copy")
        endforeach ()

        foreach (PLUGIN ${PROGRAM_PLUGINS})
            add_dependencies ("${PROGRAM}_launcher" "${PLUGIN}_dev_copy")
        endforeach ()

        add_dependencies ("${PROGRAM}_launcher"
                "${PROGRAM}_dev_configuration"
                "${APPLICATION_NAME}_dev_core_configuration")

        # Find program resource targets.

        set (RESOURCE_TARGETS)

        foreach (PLUGIN ${PROGRAM_PLUGINS})
            find_linked_targets_recursively (TARGET "${PLUGIN}_library" OUTPUT PLUGIN_TARGETS ARTEFACT_SCOPE)
            foreach (PLUGIN_TARGET ${PLUGIN_TARGETS})
                get_target_property (THIS_RESOURCE_TARGETS "${PLUGIN_TARGET}" UNIT_RESOURCE_TARGETS)
                if (NOT THIS_RESOURCE_TARGETS STREQUAL "THIS_RESOURCE_TARGETS-NOTFOUND")
                    list (APPEND RESOURCE_TARGETS ${THIS_RESOURCE_TARGETS})
                endif ()
            endforeach ()
        endforeach ()

        list (REMOVE_DUPLICATES RESOURCE_TARGETS)

        # Generate program configuration.

        get_target_property (PROGRAM_CONFIGURATION "${PROGRAM}" APPLICATION_PROGRAM_CONFIGURATION)
        if (PROGRAM_CONFIGURATION STREQUAL "PROGRAM_CONFIGURATION-NOTFOUND")
            message (FATAL_ERROR
                    "There is no program \"${PROGRAM_NAME}\" configuration in application \"${APPLICATION_NAME}\"!")
        endif ()

        set (DEV_PROGRAM_CONFIGURATOR_CONTENT)
        foreach (PLUGIN ${PROGRAM_PLUGINS})
            string (APPEND DEV_PROGRAM_CONFIGURATOR_CONTENT
                    "list (APPEND PLUGINS_LIST \"\\\"${PLUGIN}_library\\\"\")\n")
        endforeach ()

        string (APPEND DEV_PROGRAM_CONFIGURATOR_CONTENT "list (JOIN PLUGINS_LIST \", \" PLUGINS)\n")
        foreach (RESOURCE_TARGET ${RESOURCE_TARGETS})
            get_target_property (SOURCE_DIRECTORY "${RESOURCE_TARGET}" UNIT_RESOURCE_TARGET_SOURCE_DIRECTORY)
            string (APPEND DEV_PROGRAM_CONFIGURATOR_CONTENT
                    "string (APPEND RESOURCE_DIRECTORIES \"+resource_directories ")

            string (APPEND DEV_PROGRAM_CONFIGURATOR_CONTENT "{ path = \\\"${SOURCE_DIRECTORY}\\\" mount_path = ")
            string (APPEND DEV_PROGRAM_CONFIGURATOR_CONTENT
                    "\\\"${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}/${RESOURCE_TARGET}\\\" }\\n\")\n")
        endforeach ()

        set (DEV_PROGRAM_CONFIGURATION_PATH "${DEV_CONFIGURATION_DIRECTORY}/program_${PROGRAM_NAME}.rd")
        string (APPEND DEV_PROGRAM_CONFIGURATOR_CONTENT
                "configure_file (\"${PROGRAM_CONFIGURATION}\" \"${DEV_PROGRAM_CONFIGURATION_PATH}\")")

        set (DEV_PROGRAM_CONFIGURATOR_PATH
                "${DEV_BUILD_DIRECTORY}/${APPLICATION_NAME}_dev_program_${PROGRAM_NAME}_config.cmake")
        file (GENERATE OUTPUT "${DEV_PROGRAM_CONFIGURATOR_PATH}" CONTENT "${DEV_PROGRAM_CONFIGURATOR_CONTENT}")

        add_custom_target ("${PROGRAM}_dev_configuration" ALL
                DEPENDS "${APPLICATION_NAME}_prepare_dev_directories"
                COMMAND "${CMAKE_COMMAND}" -P "${DEV_PROGRAM_CONFIGURATOR_PATH}"
                COMMENT "Building program \"${PROGRAM_NAME}\" configuration for application \"${APPLICATION_NAME}\".")

    endforeach ()

    message (STATUS "Application \"${APPLICATION_NAME}\" generation done.")
endfunction ()

# Intended only for internal use in this file. Adds resource target with given name to current unit.
function (private_add_resource_target_to_unit NAME)
    get_target_property (RESOURCE_TARGETS "${UNIT_NAME}" UNIT_RESOURCE_TARGETS)
    if (RESOURCE_TARGETS STREQUAL "RESOURCE_TARGETS-NOTFOUND")
        set (RESOURCE_TARGETS)
    endif ()

    list (APPEND RESOURCE_TARGETS "${UNIT_NAME}_resource_${NAME}")
    set_target_properties ("${UNIT_NAME}" PROPERTIES UNIT_RESOURCE_TARGETS "${RESOURCE_TARGETS}")
endfunction ()

# Registers resource directory with USUAL type for current unit.
# USUAL directories share common processing pipeline for packaging.
# Arguments:
# - NAME: Logical name of this resource directory.
# - PATH: Path to resource directory with source files.
# - DEPENDENCIES: Optional dependency targets that need to be executed before processing resources for packaging.
function (register_application_usual_resource_directory)
    cmake_parse_arguments (ARGUMENT "" "NAME;PATH" "DEPENDENCIES" ${ARGV})
    if (DEFINED ARGUMENT_UNPARSED_ARGUMENTS OR
            NOT DEFINED ARGUMENT_NAME OR
            NOT DEFINED ARGUMENT_PATH)
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    cmake_path (ABSOLUTE_PATH ARGUMENT_PATH NORMALIZE)
    add_custom_target ("${UNIT_NAME}_resource_${ARGUMENT_NAME}" ALL)
    set_target_properties ("${UNIT_NAME}_resource_${ARGUMENT_NAME}" PROPERTIES
            UNIT_RESOURCE_TARGET_TYPE "USUAL"
            UNIT_RESOURCE_TARGET_SOURCE_DIRECTORY "${ARGUMENT_PATH}")

    if (DEFINED ARGUMENT_DEPENDENCIES)
        add_dependencies ("${UNIT_NAME}_resource_${ARGUMENT_NAME}" "${ARGUMENT_DEPENDENCIES}")
    endif ()

    private_add_resource_target_to_unit ("${ARGUMENT_NAME}")
    message (STATUS
            "    Added usual resource directory under name \"${ARGUMENT_NAME}\" at path \"${ARGUMENT_PATH}\".")
endfunction ()

# Registers resource directory with CUSTOM type for current unit.
# CUSTOM directories have their own commands for building files before packaging.
# Arguments:
# - NAME: Logical name of this resource directory.
# - PATH: Path to resource directory with source files.
# - BUILT_FILES: List of output files processed by custom pipeline that should be added as sources to this target.
# - DEPENDENCIES: Optional dependency targets that need to be executed before processing resources for packaging.
function (register_application_custom_resource_directory)
    cmake_parse_arguments (ARGUMENT "" "NAME;PATH" "BUILT_FILES;DEPENDENCIES" ${ARGV})
    if (DEFINED ARGUMENT_UNPARSED_ARGUMENTS OR
            NOT DEFINED ARGUMENT_NAME OR
            NOT DEFINED ARGUMENT_PATH OR
            NOT DEFINED ARGUMENT_BUILT_FILES)
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    cmake_path (ABSOLUTE_PATH ARGUMENT_PATH NORMALIZE)
    add_custom_target ("${UNIT_NAME}_resource_${ARGUMENT_NAME}" ALL)
    target_sources ("${UNIT_NAME}_resource_${ARGUMENT_NAME}" PRIVATE "${ARGUMENT_BUILT_FILES}")

    set_target_properties ("${UNIT_NAME}_resource_${ARGUMENT_NAME}" PROPERTIES
            UNIT_RESOURCE_TARGET_TYPE "CUSTOM"
            UNIT_RESOURCE_TARGET_SOURCE_DIRECTORY "${ARGUMENT_PATH}")

    if (DEFINED ARGUMENT_DEPENDENCIES)
        add_dependencies ("${UNIT_NAME}_resource_${ARGUMENT_NAME}" "${ARGUMENT_DEPENDENCIES}")
    endif ()

    private_add_resource_target_to_unit ("${ARGUMENT_NAME}")
    message (STATUS
            "    Added custom resource directory under name \"${ARGUMENT_NAME}\" at path \"${ARGUMENT_PATH}\".")
endfunction ()

# Registers resource directory with PRE_MADE type for current unit.
# PRE_MADE directories have no packaging steps as their resources are already ready for the game.
# Arguments:
# - NAME: Logical name of this resource directory.
# - PATH: Path to resource directory with source files.
# - DEPENDENCIES: Optional dependency targets that need to be executed before processing resources for packaging.
function (register_application_pre_made_resource_directory)
    cmake_parse_arguments (ARGUMENT "" "NAME;PATH" "DEPENDENCIES" ${ARGV})
    if (DEFINED ARGUMENT_UNPARSED_ARGUMENTS OR
            NOT DEFINED ARGUMENT_NAME OR
            NOT DEFINED ARGUMENT_PATH)
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    cmake_path (ABSOLUTE_PATH ARGUMENT_PATH NORMALIZE)
    add_custom_target ("${UNIT_NAME}_resource_${ARGUMENT_NAME}" ALL)
    set_target_properties ("${UNIT_NAME}_resource_${ARGUMENT_NAME}" PROPERTIES
            UNIT_RESOURCE_TARGET_TYPE "PRE_MADE"
            UNIT_RESOURCE_TARGET_SOURCE_DIRECTORY "${ARGUMENT_PATH}")

    if (DEFINED ARGUMENT_DEPENDENCIES)
        add_dependencies ("${UNIT_NAME}_resource_${ARGUMENT_NAME}" "${ARGUMENT_DEPENDENCIES}")
    endif ()

    private_add_resource_target_to_unit ("${ARGUMENT_NAME}")
    message (STATUS
            "    Added pre made resource directory under name \"${ARGUMENT_NAME}\" at path \"${ARGUMENT_PATH}\".")
endfunction ()
