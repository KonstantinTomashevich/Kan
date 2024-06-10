# Contains pipeline for creating application framework applications.

# Name of the directory to store configurations.
set (KAN_APPLICATION_CONFIGURATION_DIRECTORY_NAME "configuration")

# Name of the directory to store plugins.
set (KAN_APPLICATION_PLUGINS_DIRECTORY_NAME "plugins")

# Name of the directory to store resources.
set (KAN_APPLICATION_RESOURCES_DIRECTORY_NAME "resources")

# Name of the directory to store packaged variants of application.
set (KAN_APPLICATION_PACKAGED_DIRECTORY_NAME "packaged")

# Name of the directory to store universe world definitions in packaged variants of application.
set (KAN_APPLICATION_PACKAGED_WORLD_DIRECTORY_NAME "world")

# Path to static data template for application framework static launcher.
set (KAN_APPLICATION_PROGRAM_LAUNCHER_STATICS_TEMPLATE "${CMAKE_SOURCE_DIR}/cmake/kan/application_launcher_statics.c")

# Path to static data template for application framework tool.
set (KAN_APPLICATION_TOOL_STATICS_TEMPLATE "${CMAKE_SOURCE_DIR}/cmake/kan/application_tool_statics.c")

# Name of the used application framework static launcher implementation.
set (KAN_APPLICATION_PROGRAM_LAUNCHER_IMPLEMENTATION "sdl")

# Whether to enable auto build and hot reload commands for development builds.
option (KAN_APPLICATION_ENABLE_AUTO_BUILD_AND_HOT_RELOAD
        "Whether to enable auto build and hot reload commands for development builds." ON)

# Whether to use raw resources instead of processed ones for packing.
option (KAN_APPLICATION_PACK_WITH_RAW_RESOURCES
        "Whether to use raw resources instead of processed ones for packing." OFF)

# Whether string interning for packing procedure is enabled.
option (KAN_APPLICATION_PACKER_INTERN_STRINGS "Whether string interning for packing procedure is enabled." ON)

# Whether to observe for world definition changes in packaged applications.
option (KAN_APPLICATION_OBSERVE_WORLDS_IN_PACKAGED
        "Whether to observe for world definition changes in packaged applications." OFF)

# Whether to enable code hot reload in packaged applications.
option (KAN_APPLICATION_ENABLE_CODE_HOT_RELOAD_IN_PACKAGED
        "Whether to enable code hot reload in packaged applications." OFF)

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

define_property (TARGET PROPERTY APPLICATION_PROGRAM_USE_AS_TEST_IN_DEVELOPMENT_MODE
        BRIEF_DOCS "Whether to add variant execution in development configuration to CTest."
        FULL_DOCS "Whether to add variant execution in development configuration to CTest.")

define_property (TARGET PROPERTY APPLICATION_PROGRAM_TEST_IN_DEVELOPMENT_MODE_ARGUMENTS
        BRIEF_DOCS "Contains additional arguments for test execution in development mode."
        FULL_DOCS "Contains additional arguments for test execution in development mode.")

define_property (TARGET PROPERTY APPLICATION_PROGRAM_TEST_IN_DEVELOPMENT_MODE_PROPERTIES
        BRIEF_DOCS "Contains additional properties for test execution in development mode."
        FULL_DOCS "Contains additional properties for test execution in development mode.")

define_property (TARGET PROPERTY APPLICATION_PROGRAM_USE_AS_TEST_IN_PACKAGED_MODE
        BRIEF_DOCS "Whether to add variant execution in packaged configuration to CTest."
        FULL_DOCS "Whether to add variant execution in packaged configuration to CTest.")

define_property (TARGET PROPERTY APPLICATION_PROGRAM_TEST_IN_PACKAGED_MODE_ARGUMENTS
        BRIEF_DOCS "Contains additional arguments for test execution in packaged mode."
        FULL_DOCS "Contains additional arguments for test execution in packaged mode.")

define_property (TARGET PROPERTY APPLICATION_PROGRAM_TEST_IN_PACKAGED_MODE_PROPERTIES
        BRIEF_DOCS "Contains additional properties for test execution in packaged mode."
        FULL_DOCS "Contains additional properties for test execution in packaged mode.")

define_property (TARGET PROPERTY APPLICATION_VARIANTS
        BRIEF_DOCS "Contains list of this application packaging variants."
        FULL_DOCS "Contains list of this application packaging variants.")

define_property (TARGET PROPERTY APPLICATION_VARIANT_NAME
        BRIEF_DOCS "Contains name of application packaging variant."
        FULL_DOCS "Contains name of application packaging variant.")

define_property (TARGET PROPERTY APPLICATION_VARIANT_PROGRAMS
        BRIEF_DOCS "Contains list of programs to be packaged using application packaging variant."
        FULL_DOCS "Contains list of programs to be packaged using application packaging variant.")

define_property (TARGET PROPERTY APPLICATION_VARIANT_ENVIRONMENT_TAGS
        BRIEF_DOCS "Contains application variant environment tags."
        FULL_DOCS "Contains application variant environment tags.")

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
    add_custom_target ("${NAME}")
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

# Sets path to application world directory.
function (application_set_world_directory DIRECTORY)
    cmake_path (ABSOLUTE_PATH DIRECTORY NORMALIZE)
    set_target_properties ("${APPLICATION_NAME}" PROPERTIES APPLICATION_WORLD_DIRECTORY "${DIRECTORY}")
    message (STATUS "    Setting core world directory to \"${DIRECTORY}\".")
endfunction ()

# Adds development-only environment tag to application.
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
    add_custom_target ("${APPLICATION_NAME}_plugin_${PLUGIN_NAME}")
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
    add_custom_target ("${APPLICATION_NAME}_program_${NAME}")
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

# Makes it possible to use this program executable as test in development mode.
# Arguments:
# - ARGUMENTS: Arguments passed to program when executing it as test.
# - PROPERTIES: Properties for test setup.
function (application_program_use_as_test_in_development_mode)
    cmake_parse_arguments (TEST "" "" "ARGUMENTS;PROPERTIES" ${ARGV})
    if (DEFINED TEST_UNPARSED_ARGUMENTS)
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    message (STATUS "        Use as test in development mode.")
    message (STATUS "            Arguments: ${TEST_ARGUMENTS}.")
    message (STATUS "            Properties: ${TEST_PROPERTIES}.")

    set_target_properties ("${APPLICATION_NAME}_program_${APPLICATION_PROGRAM_NAME}" PROPERTIES
            APPLICATION_PROGRAM_USE_AS_TEST_IN_DEVELOPMENT_MODE 1
            APPLICATION_PROGRAM_TEST_IN_DEVELOPMENT_MODE_ARGUMENTS "${TEST_ARGUMENTS}"
            APPLICATION_PROGRAM_TEST_IN_DEVELOPMENT_MODE_PROPERTIES "${TEST_PROPERTIES}")
endfunction ()

# Makes it possible to use this program executable as test in packaged mode.
# Arguments:
# - ARGUMENTS: Arguments passed to program when executing it as test.
# - PROPERTIES: Properties for test setup.
function (application_program_use_as_test_in_packaged_mode)
    cmake_parse_arguments (TEST "" "" "ARGUMENTS;PROPERTIES" ${ARGV})
    if (DEFINED TEST_UNPARSED_ARGUMENTS)
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    message (STATUS "        Use as test in packaged mode.")
    message (STATUS "            Arguments: ${TEST_ARGUMENTS}.")
    message (STATUS "            Properties: ${TEST_PROPERTIES}.")

    set_target_properties ("${APPLICATION_NAME}_program_${APPLICATION_PROGRAM_NAME}" PROPERTIES
            APPLICATION_PROGRAM_USE_AS_TEST_IN_PACKAGED_MODE 1
            APPLICATION_PROGRAM_TEST_IN_PACKAGED_MODE_ARGUMENTS "${TEST_ARGUMENTS}"
            APPLICATION_PROGRAM_TEST_IN_PACKAGED_MODE_PROPERTIES "${TEST_PROPERTIES}")
endfunction ()

# Starts application packaging variant registration routine. Must be called inside application registration routine.
function (register_application_variant NAME)
    message (STATUS "    Registering variant \"${NAME}\".")
    set (APPLICATION_VARIANT_NAME "${NAME}" PARENT_SCOPE)
    add_custom_target ("${APPLICATION_NAME}_variant_${NAME}")
    set_target_properties ("${APPLICATION_NAME}_variant_${NAME}" PROPERTIES APPLICATION_VARIANT_NAME "${NAME}")

    add_dependencies ("${APPLICATION_NAME}" "${APPLICATION_NAME}_variant_${NAME}")
    get_target_property (VARIANTS "${APPLICATION_NAME}" APPLICATION_VARIANTS)

    if (VARIANTS STREQUAL "VARIANTS-NOTFOUND")
        set (VARIANTS)
    endif ()

    list (APPEND VARIANTS "${APPLICATION_NAME}_variant_${NAME}")
    set_target_properties ("${APPLICATION_NAME}" PROPERTIES APPLICATION_VARIANTS "${VARIANTS}")
endfunction ()

# Adds program with given name to application packaging variant.
function (application_variant_add_program NAME)
    get_target_property (PROGRAMS "${APPLICATION_NAME}_variant_${APPLICATION_VARIANT_NAME}"
            APPLICATION_VARIANT_PROGRAMS)

    if (PROGRAMS STREQUAL "PROGRAMS-NOTFOUND")
        set (PROGRAMS)
    endif ()

    message (STATUS "        Add program \"${NAME}\".")
    list (APPEND PROGRAMS "${APPLICATION_NAME}_program_${NAME}")
    set_target_properties ("${APPLICATION_NAME}_variant_${APPLICATION_VARIANT_NAME}" PROPERTIES
            APPLICATION_VARIANT_PROGRAMS "${PROGRAMS}")
endfunction ()

# Adds given environment tag to application packaging variant.
function (application_variant_add_environment_tag TAG)
    message (STATUS "        Adding environment tag \"${TAG}\".")
    get_target_property (TAGS "${APPLICATION_NAME}_variant_${APPLICATION_VARIANT_NAME}"
            APPLICATION_VARIANT_ENVIRONMENT_TAG)

    if (TAGS STREQUAL "TAGS-NOTFOUND")
        set (TAGS)
    endif ()

    list (APPEND TAGS "${TAG}")
    set_target_properties ("${APPLICATION_NAME}_variant_${APPLICATION_VARIANT_NAME}" PROPERTIES
            APPLICATION_VARIANT_ENVIRONMENT_TAGS "${TAGS}")
endfunction ()

# Intended only for internal use in this file.
# Gathers all resource targets used by given list of plugins and outputs resulting list to OUTPUT variable.
function (private_gather_plugins_resource_targets PLUGINS OUTPUT)
    set (FOUND_RESOURCE_TARGETS)
    foreach (PLUGIN ${PLUGINS})
        find_linked_targets_recursively (TARGET "${PLUGIN}_library" OUTPUT PLUGIN_TARGETS ARTEFACT_SCOPE)
        foreach (PLUGIN_TARGET ${PLUGIN_TARGETS})
            get_target_property (THIS_RESOURCE_TARGETS "${PLUGIN_TARGET}" UNIT_RESOURCE_TARGETS)
            if (NOT THIS_RESOURCE_TARGETS STREQUAL "THIS_RESOURCE_TARGETS-NOTFOUND")
                list (APPEND FOUND_RESOURCE_TARGETS ${THIS_RESOURCE_TARGETS})
            endif ()
        endforeach ()
    endforeach ()

    list (REMOVE_DUPLICATES FOUND_RESOURCE_TARGETS)
    set ("${OUTPUT}" "${FOUND_RESOURCE_TARGETS}" PARENT_SCOPE)
endfunction ()

# Intended only for internal use in this file.
# Gathers all plugins referenced by given list of groups and outputs resulting list to OUTPUT variable.
function (private_gather_plugins_from_groups GROUPS OUTPUT)
    get_target_property (PLUGINS "${APPLICATION_NAME}" APPLICATION_PLUGINS)
    if (PLUGINS STREQUAL "PLUGINS-NOTFOUND")
        set (PLUGINS)
    endif ()

    set (FOUND_PLUGINS)
    foreach (PLUGIN ${PLUGINS})
        get_target_property (PLUGIN_GROUP "${PLUGIN}" APPLICATION_PLUGIN_GROUP)
        if ("${PLUGIN_GROUP}" IN_LIST GROUPS)
            list (APPEND FOUND_PLUGINS "${PLUGIN}")
        endif ()
    endforeach ()

    set ("${OUTPUT}" "${FOUND_PLUGINS}" PARENT_SCOPE)
endfunction ()

# Intended only for internal use in this file.
# Generates resource preparation and packing targets with given name and using given resource targets.
function (private_generate_resource_processing NAME RESOURCE_TARGETS)
    set (RESOURCE_LIST)
    set (PREPARATION_TARGET_NAME "${APPLICATION_NAME}_resources_${NAME}_prepare")
    add_custom_target ("${PREPARATION_TARGET_NAME}")
    message (STATUS "    Generating resource processing pipeline \"${NAME}\".")

    foreach (RESOURCE_TARGET ${RESOURCE_TARGETS})
        message (STATUS "        Using resource target \"${RESOURCE_TARGET}\".")
        get_target_property (TYPE "${RESOURCE_TARGET}" UNIT_RESOURCE_TARGET_TYPE)
        get_target_property (SOURCE_DIRECTORY "${RESOURCE_TARGET}" UNIT_RESOURCE_TARGET_SOURCE_DIRECTORY)

        if (TYPE STREQUAL "USUAL")
            # Binarize readable data files, do not touch other files.
            file (GLOB_RECURSE RESOURCES LIST_DIRECTORIES false RELATIVE "${SOURCE_DIRECTORY}" "${SOURCE_DIRECTORY}/*")

            foreach (RESOURCE ${RESOURCES})
                if (RESOURCE MATCHES "\\.rd$")
                    # Readable data, we need to binarize it.
                    set (RESOURCE_ABSOLUTE_SOURCE "${SOURCE_DIRECTORY}/${RESOURCE}")
                    set (RESOURCE_ABSOLUTE_TARGET
                            "${CMAKE_CURRENT_BINARY_DIR}/Generated/${PREPARATION_TARGET_NAME}/${RESOURCE}")
                    string (REPLACE "\.rd" ".bin" RESOURCE_ABSOLUTE_TARGET "${RESOURCE_ABSOLUTE_TARGET}")

                    add_custom_command (
                            OUTPUT "${RESOURCE_ABSOLUTE_TARGET}"
                            DEPENDS "${APPLICATION_NAME}_resource_binarizer" "${RESOURCE_ABSOLUTE_SOURCE}"
                            COMMAND
                            "${APPLICATION_NAME}_resource_binarizer"
                            "${RESOURCE_ABSOLUTE_SOURCE}"
                            "${RESOURCE_ABSOLUTE_TARGET}"
                            COMMENT "Binarizing \"${RESOURCE_ABSOLUTE_SOURCE}\"."
                            VERBATIM)

                    target_sources ("${PREPARATION_TARGET_NAME}" PRIVATE "${RESOURCE_ABSOLUTE_TARGET}")
                    list (APPEND RESOURCE_LIST "${RESOURCE_ABSOLUTE_TARGET}")

                else ()
                    list (APPEND RESOURCE_LIST "${SOURCE_DIRECTORY}/${RESOURCE}")
                endif ()

            endforeach ()


        elseif (TYPE STREQUAL "CUSTOM")
            # Add dependency and add all built resources to list.
            add_dependencies ("${PREPARATION_TARGET_NAME}" "${RESOURCE_TARGET}")
            get_target_property (BUILT_RESOURCES "${RESOURCE_TARGET}" SOURCES)

            foreach (RESOURCE ${BUILT_RESOURCES})
                cmake_path (ABSOLUTE_PATH RESOURCE NORMALIZE)
                list (APPEND RESOURCE_LIST "${RESOURCE}")
            endforeach ()

        elseif (TYPE STREQUAL "PRE_MADE")
            # Just append all resources to list.
            file (GLOB_RECURSE RESOURCES LIST_DIRECTORIES false RELATIVE "${SOURCE_DIRECTORY}" "${SOURCE_DIRECTORY}/*")

            foreach (RESOURCE ${RESOURCES})
                list (APPEND RESOURCE_LIST "${SOURCE_DIRECTORY}/${RESOURCE}")
            endforeach ()

        else ()
            message (SEND_ERROR "Unknown resource target type \"${TYPE}\".")
        endif ()
    endforeach ()

    set (PACKAGING_TARGET_NAME "${APPLICATION_NAME}_resources_${NAME}_packaging")
    set (PACKAGING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Generated/${PACKAGING_TARGET_NAME}")

    list (JOIN RESOURCE_LIST "\n" RESOURCES)
    if (RESOURCES)
        message (STATUS "    Generating resource packaging step for resource processing pipeline \"${NAME}\".")
        file (WRITE "${PACKAGING_DIRECTORY}/resources.txt" "${RESOURCES}")
        set (INTERN_STRING_ARGUMENT)

        if (KAN_APPLICATION_PACKER_INTERN_STRINGS)
            set (INTERN_STRING_ARGUMENT "--intern-strings")
        endif ()

        add_custom_target ("${PACKAGING_TARGET_NAME}"
                COMMAND
                "${APPLICATION_NAME}_packer"
                "${PACKAGING_DIRECTORY}/resources.txt"
                "${PACKAGING_DIRECTORY}/${NAME}.pack"
                ${INTERN_STRING_ARGUMENT}
                WORKING_DIRECTORY ${PACKAGING_DIRECTORY}
                VERBATIM)
        add_dependencies ("${PACKAGING_TARGET_NAME}" "${PREPARATION_TARGET_NAME}")
    endif ()
endfunction ()

# Uses data gathered by registration functions above to generate application shared libraries, executables and other
# application related targets.
function (application_generate)
    message (STATUS "Application \"${APPLICATION_NAME}\" registration done, generating...")
    message (STATUS "Application \"${APPLICATION_NAME}\": generating development targets.")

    # Find core plugins, we'll need them later.

    set (CORE_PLUGINS)
    get_target_property (PLUGINS "${APPLICATION_NAME}" APPLICATION_PLUGINS)

    if (PLUGINS STREQUAL "PLUGINS-NOTFOUND")
        set (PLUGINS)
    endif ()

    get_target_property (CORE_GROUPS "${APPLICATION_NAME}" APPLICATION_CORE_PLUGIN_GROUPS)
    if (NOT CORE_GROUPS STREQUAL "CORE_GROUPS-NOTFOUND")
        private_gather_plugins_from_groups ("${CORE_GROUPS}" CORE_PLUGINS)
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

    add_custom_target ("${APPLICATION_NAME}_prepare_dev_directories"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEV_CONFIGURATION_DIRECTORY}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEV_PLUGINS_DIRECTORY}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEV_RESOURCES_DIRECTORY}"
            COMMENT "Creating development build directories for application \"${APPLICATION_NAME}\".")

    # Reserve target for plugin build for hot reloading.

    add_custom_target ("${APPLICATION_NAME}_dev_all_plugins")

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

        add_dependencies ("${APPLICATION_NAME}_dev_all_plugins" "${PLUGIN}_dev_copy")
    endforeach ()

    # Find core resource targets.

    set (CORE_RESOURCE_TARGETS)
    private_gather_plugins_resource_targets ("${CORE_PLUGINS}" "CORE_RESOURCE_TARGETS")

    find_linked_targets_recursively (TARGET "${APPLICATION_NAME}_core_library" OUTPUT CORE_LINKED_TARGETS)
    foreach (LINKED_TARGET ${CORE_LINKED_TARGETS})
        get_target_property (THIS_RESOURCE_TARGETS "${LINKED_TARGET}" UNIT_RESOURCE_TARGETS)
        if (NOT THIS_RESOURCE_TARGETS STREQUAL "THIS_RESOURCE_TARGETS-NOTFOUND")
            list (APPEND CORE_RESOURCE_TARGETS ${THIS_RESOURCE_TARGETS})
        endif ()
    endforeach ()

    list (REMOVE_DUPLICATES CORE_RESOURCE_TARGETS)

    # Generate development core configuration.

    get_target_property (CORE_CONFIGURATION "${APPLICATION_NAME}" APPLICATION_CORE_CONFIGURATION)
    if (CORE_CONFIGURATION STREQUAL "CORE_CONFIGURATION-NOTFOUND")
        message (FATAL_ERROR "There is no core configuration for application \"${APPLICATION_NAME}\"!")
    endif ()

    get_target_property (WORLD_DIRECTORY "${APPLICATION_NAME}" APPLICATION_WORLD_DIRECTORY)
    if (WORLD_DIRECTORY STREQUAL "WORLD_DIRECTORY-NOTFOUND")
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
            "set (WORLDS_DIRECTORY_PATH \"${WORLD_DIRECTORY}\")\n")
    string (APPEND DEV_CORE_CONFIGURATOR_CONTENT "set (OBSERVE_WORLD_DEFINITIONS 1)\n")
    string (APPEND DEV_CORE_CONFIGURATOR_CONTENT "set (ENABLE_CODE_HOT_RELOAD 1)\n")

    if (KAN_APPLICATION_ENABLE_AUTO_BUILD_AND_HOT_RELOAD)
        string (APPEND DEV_CORE_CONFIGURATOR_CONTENT "set (AUTO_CODE_HOT_RELOAD_COMMAND ")
        string (APPEND DEV_CORE_CONFIGURATOR_CONTENT
                "\"auto_build_and_hot_reload_command = \\\"${CMAKE_COMMAND} ")
        string (APPEND DEV_CORE_CONFIGURATOR_CONTENT "--build \\\\\\\"${CMAKE_BINARY_DIR}\\\\\\\" ")
        string (APPEND DEV_CORE_CONFIGURATOR_CONTENT
                "--target \\\\\\\"${APPLICATION_NAME}_dev_all_plugins\\\\\\\"\\\"\")\n")
    endif ()

    set (DEV_CORE_CONFIGURATION_PATH "${DEV_CONFIGURATION_DIRECTORY}/core.rd")
    string (APPEND DEV_CORE_CONFIGURATOR_CONTENT
            "configure_file (\"${CORE_CONFIGURATION}\" \"${DEV_CORE_CONFIGURATION_PATH}\")")

    set (DEV_CORE_CONFIGURATOR_PATH "${DEV_BUILD_DIRECTORY}/${APPLICATION_NAME}_dev_core_config.cmake")
    file (GENERATE OUTPUT "${DEV_CORE_CONFIGURATOR_PATH}" CONTENT "${DEV_CORE_CONFIGURATOR_CONTENT}")

    add_custom_target ("${APPLICATION_NAME}_dev_core_configuration"
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
            private_gather_plugins_from_groups ("${PROGRAM_GROUPS}" PROGRAM_PLUGINS)
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

        private_gather_plugins_resource_targets ("${PROGRAM_PLUGINS}" "RESOURCE_TARGETS")

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

        add_custom_target ("${PROGRAM}_dev_configuration"
                DEPENDS "${APPLICATION_NAME}_prepare_dev_directories"
                COMMAND "${CMAKE_COMMAND}" -P "${DEV_PROGRAM_CONFIGURATOR_PATH}"
                COMMENT "Building program \"${PROGRAM_NAME}\" configuration for application \"${APPLICATION_NAME}\".")

        # Add program executable to tests if request.

        get_target_property (IS_TEST "${PROGRAM}" APPLICATION_PROGRAM_USE_AS_TEST_IN_DEVELOPMENT_MODE)
        if (${IS_TEST})
            get_target_property (TEST_ARGUMENTS "${PROGRAM}" APPLICATION_PROGRAM_TEST_IN_DEVELOPMENT_MODE_ARGUMENTS)
            if (TEST_ARGUMENTS STREQUAL "TEST_ARGUMENTS-NOTFOUND")
                set (TEST_ARGUMENTS)
            endif ()

            get_target_property (TEST_PROPERTIES "${PROGRAM}" APPLICATION_PROGRAM_TEST_IN_DEVELOPMENT_MODE_PROPERTIES)
            if (TEST_PROPERTIES STREQUAL "TEST_PROPERTIES-NOTFOUND")
                unset (TEST_PROPERTIES)
            endif ()

            add_test (NAME "${PROGRAM}_test_in_development"
                    COMMAND "${PROGRAM}_launcher" ${TEST_ARGUMENTS}
                    WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROGRAM}_launcher>"
                    COMMAND_EXPAND_LISTS)

            if (DEFINED TEST_PROPERTIES)
                message (STATUS "$$$ ${TEST_PROPERTIES}")
                set_tests_properties ("${PROGRAM}_test_in_development" PROPERTIES ${TEST_PROPERTIES})
            endif ()

            add_dependencies (test_kan "${PROGRAM}_launcher")
        endif ()

    endforeach ()

    if (NOT KAN_APPLICATION_PACK_WITH_RAW_RESOURCES)
        message (STATUS "Application \"${APPLICATION_NAME}\": generating resource processing targets.")

        private_generate_resource_processing ("core" "${CORE_RESOURCE_TARGETS}")
        foreach (PLUGIN ${PLUGINS})
            if (NOT PLUGIN IN_LIST CORE_PLUGINS)
                find_linked_targets_recursively (TARGET "${PLUGIN}_library" OUTPUT PLUGIN_TARGETS ARTEFACT_SCOPE)
                set (RESOURCE_TARGETS)

                foreach (PLUGIN_TARGET ${PLUGIN_TARGETS})
                    get_target_property (THIS_RESOURCE_TARGETS "${PLUGIN_TARGET}" UNIT_RESOURCE_TARGETS)
                    if (NOT THIS_RESOURCE_TARGETS STREQUAL "THIS_RESOURCE_TARGETS-NOTFOUND")
                        list (APPEND RESOURCE_TARGETS ${THIS_RESOURCE_TARGETS})
                    endif ()
                endforeach ()

                get_target_property (NAME "${PLUGIN}" APPLICATION_PLUGIN_NAME)
                private_generate_resource_processing ("${NAME}" "${RESOURCE_TARGETS}")
            endif ()
        endforeach ()
    endif ()

    message (STATUS "Application \"${APPLICATION_NAME}\": generating packaging variants.")

    get_target_property (VARIANTS "${APPLICATION_NAME}" APPLICATION_VARIANTS)
    if (VARIANTS STREQUAL "VARIANTS-NOTFOUND")
        message (STATUS "    Application \"${APPLICATION_NAME}\" has no packaging variants.")
        set (VARIANTS)
    endif ()

    # Generate variant package targets.

    foreach (VARIANT ${VARIANTS})
        get_target_property (NAME "${VARIANT}" APPLICATION_VARIANT_NAME)
        message (STATUS "    Generating variant \"${NAME}\".")
        add_custom_target ("${VARIANT}_package")

        # Set packaging directories.

        set (PACK_BUILD_DIRECTORY
                "${CMAKE_CURRENT_BINARY_DIR}/${KAN_APPLICATION_PACKAGED_DIRECTORY_NAME}/${APPLICATION_NAME}/${NAME}")
        set (PACK_CONFIGURATION_DIRECTORY "${PACK_BUILD_DIRECTORY}/${KAN_APPLICATION_CONFIGURATION_DIRECTORY_NAME}")
        set (PACK_PLUGINS_DIRECTORY "${PACK_BUILD_DIRECTORY}/${KAN_APPLICATION_PLUGINS_DIRECTORY_NAME}")
        set (PACK_RESOURCES_DIRECTORY "${PACK_BUILD_DIRECTORY}/${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}")

        add_custom_target ("${VARIANT}_prepare_directories"
                COMMAND "${CMAKE_COMMAND}" -E rm -rf "${PACK_BUILD_DIRECTORY}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${PACK_CONFIGURATION_DIRECTORY}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${PACK_PLUGINS_DIRECTORY}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${PACK_RESOURCES_DIRECTORY}"
                COMMENT "Creating packaging directories for application \"${APPLICATION_NAME}\" variant \"${NAME}\".")

        # Copy core library.

        setup_shared_library_copy (
                LIBRARY "${APPLICATION_NAME}_core_library"
                USER "${VARIANT}_package"
                OUTPUT ${PACK_BUILD_DIRECTORY}
                DEPENDENCIES "${VARIANT}_prepare_directories")

        find_linked_shared_libraries (TARGET "${APPLICATION_NAME}_core_library" OUTPUT CORE_REQUIRED_LIBRARIES)
        foreach (REQUIRED_LIBRARY ${CORE_REQUIRED_LIBRARIES})
            setup_shared_library_copy (
                    LIBRARY "${REQUIRED_LIBRARY}"
                    USER "${VARIANT}_package"
                    OUTPUT ${PACK_BUILD_DIRECTORY}
                    DEPENDENCIES "${VARIANT}_prepare_directories")
        endforeach ()

        # Find plugins used by variant.

        set (USED_PLUGINS ${CORE_PLUGINS})
        get_target_property (VARIANT_PROGRAMS "${VARIANT}" APPLICATION_VARIANT_PROGRAMS)

        if (VARIANT_PROGRAMS STREQUAL "VARIANT_PROGRAMS-NOTFOUND")
            message (FATAL_ERROR "Application \"${APPLICATION_NAME}\" variant \"${NAME}\" has no programs!")
        endif ()

        foreach (PROGRAM ${VARIANT_PROGRAMS})
            get_target_property (PROGRAM_GROUPS "${PROGRAM}" APPLICATION_PROGRAM_PLUGIN_GROUPS)
            if (NOT PROGRAM_GROUPS STREQUAL "PROGRAM_GROUPS-NOTFOUND")
                foreach (PLUGIN ${PLUGINS})
                    get_target_property (PLUGIN_GROUP "${PLUGIN}" APPLICATION_PLUGIN_GROUP)
                    if ("${PLUGIN_GROUP}" IN_LIST PROGRAM_GROUPS)
                        list (APPEND USED_PLUGINS "${PLUGIN}")
                    endif ()
                endforeach ()
            endif ()
        endforeach ()

        list (REMOVE_DUPLICATES USED_PLUGINS)

        # Copy plugins used by variant.

        foreach (PLUGIN ${USED_PLUGINS})
            setup_shared_library_copy (
                    LIBRARY "${PLUGIN}_library"
                    USER "${VARIANT}_package"
                    OUTPUT ${PACK_PLUGINS_DIRECTORY}
                    DEPENDENCIES "${VARIANT}_prepare_directories")
        endforeach ()

        # Generate core configuration.

        get_target_property (TAGS "${VARIANT}" APPLICATION_VARIANT_ENVIRONMENT_TAGS)
        if (TAGS STREQUAL "TAGS-NOTFOUND")
            set (TAGS)
        endif ()

        set (PACK_CORE_CONFIGURATOR_CONTENT)
        foreach (PLUGIN ${CORE_PLUGINS})
            string (APPEND PACK_CORE_CONFIGURATOR_CONTENT "list (APPEND PLUGINS_LIST \"\\\"${PLUGIN}_library\\\"\")\n")
        endforeach ()

        string (APPEND PACK_CORE_CONFIGURATOR_CONTENT "list (JOIN PLUGINS_LIST \", \" PLUGINS)\n")
        if (KAN_APPLICATION_PACK_WITH_RAW_RESOURCES)
            foreach (RESOURCE_TARGET ${CORE_RESOURCE_TARGETS})
                string (APPEND PACK_CORE_CONFIGURATOR_CONTENT
                        "string (APPEND RESOURCE_DIRECTORIES \"+resource_directories ")
                string (APPEND PACK_CORE_CONFIGURATOR_CONTENT
                        "{ path = \\\"${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}/${RESOURCE_TARGET}\\\"")
                string (APPEND PACK_CORE_CONFIGURATOR_CONTENT
                        "mount_path = \\\"${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}/${RESOURCE_TARGET}\\\" }\\n\")\n")
            endforeach ()

        elseif (TARGET "${APPLICATION_NAME}_resources_core_packaging")
            string (APPEND PACK_CORE_CONFIGURATOR_CONTENT "string (APPEND RESOURCE_PACKS \"+resource_packs ")
            string (APPEND PACK_CORE_CONFIGURATOR_CONTENT
                    "{ path = \\\"${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}/core.pack\\\"")
            string (APPEND PACK_CORE_CONFIGURATOR_CONTENT
                    " mount_path = \\\"${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}/core\\\" }\\n\")\n")
        endif ()

        foreach (TAG ${TAGS})
            string (APPEND PACK_CORE_CONFIGURATOR_CONTENT "list (APPEND ENVIRONMENT_TAGS_LIST \"\\\"${TAG}\\\"\")\n")
        endforeach ()

        string (APPEND PACK_CORE_CONFIGURATOR_CONTENT "list (JOIN ENVIRONMENT_TAGS_LIST \", \" ENVIRONMENT_TAGS)\n")
        string (APPEND PACK_CORE_CONFIGURATOR_CONTENT
                "set (PLUGINS_DIRECTORY_PATH \"${KAN_APPLICATION_PLUGINS_DIRECTORY_NAME}\")\n")
        string (APPEND PACK_CORE_CONFIGURATOR_CONTENT
                "set (WORLDS_DIRECTORY_PATH \"${KAN_APPLICATION_PACKAGED_WORLD_DIRECTORY_NAME}\")\n")

        if (KAN_APPLICATION_OBSERVE_WORLDS_IN_PACKAGED)
            string (APPEND PACK_CORE_CONFIGURATOR_CONTENT "set (OBSERVE_WORLD_DEFINITIONS 1)\n")
        else ()
            string (APPEND PACK_CORE_CONFIGURATOR_CONTENT "set (OBSERVE_WORLD_DEFINITIONS 0)\n")
        endif ()

        if (KAN_APPLICATION_ENABLE_CODE_HOT_RELOAD_IN_PACKAGED)
            string (APPEND PACK_CORE_CONFIGURATOR_CONTENT "set (ENABLE_CODE_HOT_RELOAD 1)\n")
        else ()
            string (APPEND PACK_CORE_CONFIGURATOR_CONTENT "set (ENABLE_CODE_HOT_RELOAD 0)\n")
        endif ()

        set (PACK_CORE_CONFIGURATION_PATH "${PACK_CONFIGURATION_DIRECTORY}/core.rd")
        string (APPEND PACK_CORE_CONFIGURATOR_CONTENT
                "configure_file (\"${CORE_CONFIGURATION}\" \"${PACK_CORE_CONFIGURATION_PATH}\")")

        set (PACK_CORE_CONFIGURATOR_PATH
                "${CMAKE_CURRENT_BINARY_DIR}/Generated/${APPLICATION_NAME}_variant_${NAME}_core_config.cmake")
        file (WRITE "${PACK_CORE_CONFIGURATOR_PATH}" "${PACK_CORE_CONFIGURATOR_CONTENT}")

        add_custom_target ("${VARIANT}_core_configuration"
                DEPENDS "${VARIANT}_prepare_directories"
                COMMAND "${CMAKE_COMMAND}" -P "${PACK_CORE_CONFIGURATOR_PATH}"
                COMMENT "Building core configuration for application \"${APPLICATION_NAME}\" variant \"${NAME}\".")
        add_dependencies ("${VARIANT}_package" "${VARIANT}_core_configuration")

        # Generate program configuration.

        foreach (PROGRAM ${VARIANT_PROGRAMS})
            get_target_property (PROGRAM_NAME "${PROGRAM}" APPLICATION_PROGRAM_NAME)
            set (PROGRAM_PLUGINS)
            get_target_property (PROGRAM_GROUPS "${PROGRAM}" APPLICATION_PROGRAM_PLUGIN_GROUPS)

            if (NOT PROGRAM_GROUPS STREQUAL "PROGRAM_GROUPS-NOTFOUND")
                private_gather_plugins_from_groups ("${PROGRAM_GROUPS}" PROGRAM_PLUGINS)
            endif ()

            get_target_property (PROGRAM_CONFIGURATION "${PROGRAM}" APPLICATION_PROGRAM_CONFIGURATION)
            if (PROGRAM_CONFIGURATION STREQUAL "PROGRAM_CONFIGURATION-NOTFOUND")
                message (FATAL_ERROR
                        "There is no program \"${PROGRAM_NAME}\" configuration in application \"${APPLICATION_NAME}\"!")
            endif ()

            set (PACK_PROGRAM_CONFIGURATOR_CONTENT)
            foreach (PLUGIN ${PROGRAM_PLUGINS})
                string (APPEND PACK_PROGRAM_CONFIGURATOR_CONTENT
                        "list (APPEND PLUGINS_LIST \"\\\"${PLUGIN}_library\\\"\")\n")
            endforeach ()

            string (APPEND PACK_PROGRAM_CONFIGURATOR_CONTENT "list (JOIN PLUGINS_LIST \", \" PLUGINS)\n")
            if (KAN_APPLICATION_PACK_WITH_RAW_RESOURCES)
                private_gather_plugins_resource_targets ("${PROGRAM_PLUGINS}" "RESOURCE_TARGETS")
                foreach (RESOURCE_TARGET ${RESOURCE_TARGETS})
                    set (RELATIVE_PATH "${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}/${RESOURCE_TARGET}")
                    string (APPEND PACK_PROGRAM_CONFIGURATOR_CONTENT
                            "string (APPEND RESOURCE_DIRECTORIES \"+resource_directories ")
                    string (APPEND PACK_PROGRAM_CONFIGURATOR_CONTENT
                            "{ path = \\\"${RELATIVE_PATH}\\\" mount_path = \\\"${RELATIVE_PATH}\\\" }\\n\")\n")
                endforeach ()

            else ()
                foreach (PLUGIN ${PROGRAM_PLUGINS})
                    get_target_property (PLUGIN_NAME "${PLUGIN}" APPLICATION_PLUGIN_NAME)
                    set (PACK_PATH "${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}/${PLUGIN_NAME}.pack")
                    set (MOUNT_PATH "${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}/${PLUGIN_NAME}")

                    string (APPEND PACK_PROGRAM_CONFIGURATOR_CONTENT "string (APPEND RESOURCE_PACKS \"+resource_packs ")
                    string (APPEND PACK_PROGRAM_CONFIGURATOR_CONTENT
                            "{ path = \\\"${PACK_PATH}\\\" mount_path = \\\"${MOUNT_PATH}\\\" }\\n\")\n")
                endforeach ()
            endif ()

            set (PACK_PROGRAM_CONFIGURATION_PATH "${PACK_CONFIGURATION_DIRECTORY}/program_${PROGRAM_NAME}.rd")
            string (APPEND PACK_PROGRAM_CONFIGURATOR_CONTENT
                    "configure_file (\"${PROGRAM_CONFIGURATION}\" \"${PACK_PROGRAM_CONFIGURATION_PATH}\")")

            set (GENERATED_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Generated")
            set (PACK_PROGRAM_CONFIGURATOR_PATH
                    "${GENERATED_DIRECTORY}/${APPLICATION_NAME}_variant_${NAME}_${PROGRAM_NAME}_config.cmake")
            file (WRITE "${PACK_PROGRAM_CONFIGURATOR_PATH}" "${PACK_PROGRAM_CONFIGURATOR_CONTENT}")

            set (COMMENT_PREFIX "Building program \"${PROGRAM_NAME}\" configuration for application ")
            set (COMMENT_SUFFIX "\"${APPLICATION_NAME}\" variant \"${NAME}\".")

            add_custom_target ("${VARIANT}_program_${PROGRAM_NAME}_configuration"
                    DEPENDS "${VARIANT}_prepare_directories"
                    COMMAND "${CMAKE_COMMAND}" -P "${PACK_PROGRAM_CONFIGURATOR_PATH}"
                    COMMENT "${COMMENT_PREFIX}${COMMENT_SUFFIX}")

            add_dependencies ("${VARIANT}_package" "${VARIANT}_program_${PROGRAM_NAME}_configuration")
        endforeach ()

        # Copy worlds.

        add_custom_target ("${VARIANT}_copy_worlds"
                DEPENDS "${VARIANT}_prepare_directories"
                COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${WORLD_DIRECTORY}" "${PACK_BUILD_DIRECTORY}/${KAN_APPLICATION_PACKAGED_WORLD_DIRECTORY_NAME}"
                COMMENT "Copying worlds for application \"${APPLICATION_NAME}\" variant \"${NAME}\"."
                VERBATIM)
        add_dependencies ("${VARIANT}_package" "${VARIANT}_copy_worlds")

        # Copy resources.

        if (KAN_APPLICATION_PACK_WITH_RAW_RESOURCES)
            set (RESOURCE_TARGETS ${CORE_RESOURCE_TARGETS})

            foreach (PLUGIN ${USED_PLUGINS})
                find_linked_targets_recursively (TARGET "${PLUGIN}_library" OUTPUT PLUGIN_TARGETS ARTEFACT_SCOPE)
                if (NOT PLUGIN IN_LIST CORE_PLUGINS)
                    foreach (PLUGIN_TARGET ${PLUGIN_TARGETS})
                        get_target_property (THIS_RESOURCE_TARGETS "${PLUGIN_TARGET}" UNIT_RESOURCE_TARGETS)
                        if (NOT THIS_RESOURCE_TARGETS STREQUAL "THIS_RESOURCE_TARGETS-NOTFOUND")
                            list (APPEND RESOURCE_TARGETS ${THIS_RESOURCE_TARGETS})
                        endif ()
                    endforeach ()
                endif ()
            endforeach ()

            list (REMOVE_DUPLICATES RESOURCE_TARGETS)
            foreach (RESOURCE_TARGET ${RESOURCE_TARGETS})
                get_target_property (SOURCE_DIRECTORY "${RESOURCE_TARGET}" UNIT_RESOURCE_TARGET_SOURCE_DIRECTORY)
                set (COMMENT_PREFIX "Copying \"${RESOURCE_TARGET}\" resources for application ")
                set (COMMENT_SUFFIX "\"${APPLICATION_NAME}\" variant \"${NAME}\".")

                add_custom_target ("${VARIANT}_copy_${RESOURCE_TARGET}"
                        DEPENDS "${VARIANT}_prepare_directories"
                        COMMAND
                        "${CMAKE_COMMAND}"
                        -E copy_directory
                        "${SOURCE_DIRECTORY}"
                        "${PACK_RESOURCES_DIRECTORY}/${RESOURCE_TARGET}"
                        COMMENT "${COMMENT_PREFIX}${COMMENT_SUFFIX}"
                        VERBATIM)

                add_dependencies ("${VARIANT}_package" "${VARIANT}_copy_${RESOURCE_TARGET}")
            endforeach ()

        else ()
            if (TARGET "${APPLICATION_NAME}_resources_core_packaging")
                set (SOURCE_DIRECTORY
                        "${CMAKE_CURRENT_BINARY_DIR}/Generated/${APPLICATION_NAME}_resources_core_packaging")

                add_custom_target ("${VARIANT}_copy_core_pack"
                        DEPENDS "${VARIANT}_prepare_directories" "${APPLICATION_NAME}_resources_core_packaging"
                        COMMAND
                        "${CMAKE_COMMAND}"
                        -E copy -t
                        "${PACK_RESOURCES_DIRECTORY}"
                        "${SOURCE_DIRECTORY}/core.pack"
                        COMMENT "Copying core resources for application \"${APPLICATION_NAME}\" variant \"${NAME}\"."
                        VERBATIM)

                add_dependencies ("${VARIANT}_package" "${VARIANT}_copy_core_pack")
            endif ()

            foreach (PLUGIN ${USED_PLUGINS})
                if (NOT PLUGIN IN_LIST CORE_PLUGINS)
                    get_target_property (PLUGIN_NAME "${PLUGIN}" APPLICATION_PLUGIN_NAME)
                    set (PLUGIN_TARGET_NAME "${APPLICATION_NAME}_resources_${PLUGIN_NAME}_packaging")

                    set (SOURCE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Generated/${PLUGIN_TARGET_NAME}")
                    set (COMMENT_PREFIX "Copying plugin ${PLUGIN_NAME} resources for application ")
                    set (COMMENT_SUFFIX "\"${APPLICATION_NAME}\" variant \"${NAME}\".")

                    add_custom_target ("${VARIANT}_copy_${PLUGIN_NAME}_pack"
                            DEPENDS "${VARIANT}_prepare_directories" "${PLUGIN_TARGET_NAME}"
                            COMMAND
                            "${CMAKE_COMMAND}"
                            -E copy -t
                            "${PACK_RESOURCES_DIRECTORY}"
                            "${SOURCE_DIRECTORY}/${PLUGIN_NAME}.pack"
                            COMMENT "${COMMENT_PREFIX}${COMMENT_SUFFIX}"
                            VERBATIM)

                    add_dependencies ("${VARIANT}_package" "${VARIANT}_copy_${PLUGIN_NAME}_pack")
                endif ()
            endforeach ()
        endif ()

        # Copy launchers.

        foreach (PROGRAM ${VARIANT_PROGRAMS})
            get_target_property (PROGRAM_NAME "${PROGRAM}" APPLICATION_PROGRAM_NAME)
            add_custom_target ("${VARIANT}_copy_launcher_${PROGRAM_NAME}"
                    DEPENDS "${VARIANT}_prepare_directories" "${PROGRAM}_launcher"
                    COMMAND
                    ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${PROGRAM}_launcher>"
                    "${PACK_BUILD_DIRECTORY}"
                    COMMENT
                    "Copying program \"${PROGRAM_NAME}\" launcher for \"${APPLICATION_NAME}\" variant \"${NAME}\"."
                    VERBATIM)

            add_dependencies ("${VARIANT}_package" "${VARIANT}_copy_launcher_${PROGRAM_NAME}")

            # Add program executable to tests if request.

            get_target_property (IS_TEST "${PROGRAM}" APPLICATION_PROGRAM_USE_AS_TEST_IN_PACKAGED_MODE)
            if (${IS_TEST})
                get_target_property (TEST_ARGUMENTS "${PROGRAM}" APPLICATION_PROGRAM_TEST_IN_PACKAGED_MODE_ARGUMENTS)
                if (TEST_ARGUMENTS STREQUAL "TEST_ARGUMENTS-NOTFOUND")
                    set (TEST_ARGUMENTS)
                endif ()

                get_target_property (TEST_PROPERTIES "${PROGRAM}" APPLICATION_PROGRAM_TEST_IN_PACKAGED_MODE_PROPERTIES)
                if (TEST_PROPERTIES STREQUAL "TEST_PROPERTIES-NOTFOUND")
                    unset (TEST_PROPERTIES)
                endif ()

                add_test (NAME "${PROGRAM}_test_in_packaged"
                        COMMAND "${PACK_BUILD_DIRECTORY}/$<TARGET_FILE_NAME:${PROGRAM}_launcher>" ${TEST_ARGUMENTS}
                        WORKING_DIRECTORY "${PACK_BUILD_DIRECTORY}"
                        COMMAND_EXPAND_LISTS)

                if (DEFINED TEST_PROPERTIES)
                    message (STATUS "$$$ ${TEST_PROPERTIES}")
                    set_tests_properties ("${PROGRAM}_test_in_packaged" PROPERTIES ${TEST_PROPERTIES})
                endif ()

                add_dependencies (test_kan "${VARIANT}_package")
            endif ()
        endforeach ()

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
    add_custom_target ("${UNIT_NAME}_resource_${ARGUMENT_NAME}")
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
    add_custom_target ("${UNIT_NAME}_resource_${ARGUMENT_NAME}")
    target_sources ("${UNIT_NAME}_resource_${ARGUMENT_NAME}" PRIVATE ${ARGUMENT_BUILT_FILES})

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
    add_custom_target ("${UNIT_NAME}_resource_${ARGUMENT_NAME}")
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
