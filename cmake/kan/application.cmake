# Contains pipeline for creating application framework applications.

# Name of the directory to store configurations.
set (KAN_APPLICATION_CONFIGURATION_DIRECTORY_NAME "configuration")

# Name of the directory to store plugins.
set (KAN_APPLICATION_PLUGINS_DIRECTORY_NAME "plugins")

# Name of the directory to store resources.
set (KAN_APPLICATION_RESOURCES_DIRECTORY_NAME "resources")

# Name of the build/editor-time directory to be used as storage resource reference caches.
set (KAN_APPLICATION_RC_DIRECTORY_NAME "reference_cache")

# Name of the build-time directory to be used as workspace for resource builder.
set (KAN_APPLICATION_RBW_DIRECTORY_NAME "resource_builder_workspace")

# Name of the directory to store packaged variants of application.
set (KAN_APPLICATION_PACKAGED_DIRECTORY_NAME "packaged")

# Name of the directory to store universe world definitions in packaged variants of application.
set (KAN_APPLICATION_PACKAGED_WORLD_DIRECTORY_NAME "world")

# Path to static data template for application framework static launcher.
set (KAN_APPLICATION_PROGRAM_LAUNCHER_STATICS_TEMPLATE "${CMAKE_SOURCE_DIR}/cmake/kan/application_launcher_statics.c")

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

# Whether to enable code hot reload verification target generation.
option (KAN_APPLICATION_GENERATE_CODE_HOT_RELOAD_TEST
        "Whether to enable code hot reload verification target generation." ON)

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
        BRIEF_DOCS "Whether to add program execution in development configuration to CTest."
        FULL_DOCS "Whether to add program execution in development configuration to CTest.")

define_property (TARGET PROPERTY APPLICATION_PROGRAM_TEST_IN_DEVELOPMENT_MODE_ARGUMENTS
        BRIEF_DOCS "Contains additional arguments for test execution in development mode."
        FULL_DOCS "Contains additional arguments for test execution in development mode.")

define_property (TARGET PROPERTY APPLICATION_PROGRAM_TEST_IN_DEVELOPMENT_MODE_PROPERTIES
        BRIEF_DOCS "Contains additional properties for test execution in development mode."
        FULL_DOCS "Contains additional properties for test execution in development mode.")

define_property (TARGET PROPERTY APPLICATION_PROGRAM_USE_AS_TEST_IN_PACKAGED_MODE
        BRIEF_DOCS "Whether to add program execution in packaged configuration to CTest."
        FULL_DOCS "Whether to add program execution in packaged configuration to CTest.")

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

define_property (TARGET PROPERTY UNIT_RESOURCE_DIRECTORIES
        BRIEF_DOCS "List of resource directories that are used by this unit."
        FULL_DOCS "List of resource directories that are used by this unit.")

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

    if (KAN_APPLICATION_GENERATE_CODE_HOT_RELOAD_TEST)
        message (STATUS "        Adding code hot reload test world.")
        if (NOT EXISTS "${DIRECTORY}/optional")
            file (MAKE_DIRECTORY "${DIRECTORY}/optional")
        endif ()

        file (COPY_FILE
                "${CMAKE_SOURCE_DIR}/cmake/kan/verify_code_hot_reload_world.rd"
                "${DIRECTORY}/optional/verify_code_hot_reload.rd")

    else ()
        message (STATUS "        Removing code hot reload test world.")
        file (REMOVE "${DIRECTORY}/optional/verify_code_hot_reload.rd")
    endif ()
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
# Gathers all resource directories used by given list of plugins and outputs resulting list to OUTPUT variable.
function (private_gather_plugins_resource_directories PLUGINS OUTPUT)
    set (FOUND_RESOURCE_DIRECTORIES)
    foreach (PLUGIN ${PLUGINS})
        find_linked_targets_recursively (TARGET "${PLUGIN}_library" OUTPUT PLUGIN_TARGETS ARTEFACT_SCOPE)
        foreach (PLUGIN_TARGET ${PLUGIN_TARGETS})
            get_target_property (THIS_RESOURCE_DIRECTORIES "${PLUGIN_TARGET}" UNIT_RESOURCE_DIRECTORIES)
            if (NOT THIS_RESOURCE_DIRECTORIES STREQUAL "THIS_RESOURCE_DIRECTORIES-NOTFOUND")
                list (APPEND FOUND_RESOURCE_DIRECTORIES ${THIS_RESOURCE_DIRECTORIES})
            endif ()
        endforeach ()
    endforeach ()

    list (REMOVE_DUPLICATES FOUND_RESOURCE_DIRECTORIES)
    set ("${OUTPUT}" "${FOUND_RESOURCE_DIRECTORIES}" PARENT_SCOPE)
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
# Adds test program that ensures that:
# - There is nothing in this application that breaks hot reload for trivial change.
# - This application reflection structure is coherent and hot reloadable when all plugins are loaded.
#   For example, if someone forgot to ignore kan_atomic_int_t field somewhere, this would fail.
function (private_generate_code_hot_reload_test)
    message (STATUS "Application \"${APPLICATION_NAME}\" generates test program for code hot reload verification...")

    # Register verification plugin.

    register_application_plugin (NAME verify_code_hot_reload GROUP verify_code_hot_reload)
    application_plugin_include (CONCRETE "application_framework_verify_code_hot_reload")

    # Get all plugin groups as we need to verify that reflection structure is okay in every plugin.

    set (PLUGIN_GROUPS)
    get_target_property (PLUGINS "${APPLICATION_NAME}" APPLICATION_PLUGINS)

    if (PLUGINS STREQUAL "PLUGINS-NOTFOUND")
        set (PLUGINS)
    endif ()

    get_target_property (CORE_GROUPS "${APPLICATION_NAME}" APPLICATION_CORE_PLUGIN_GROUPS)
    if (CORE_GROUPS STREQUAL "CORE_GROUPS-NOTFOUND")
        set (CORE_GROUPS)
    endif ()

    foreach (PLUGIN ${PLUGINS})
        get_target_property (PLUGIN_GROUP "${PLUGIN}" APPLICATION_PLUGIN_GROUP)
        if (NOT "${PLUGIN_GROUP}" IN_LIST CORE_GROUPS)
            list (APPEND PLUGIN_GROUPS "${PLUGIN_GROUP}")
        endif ()
    endforeach ()

    list (REMOVE_DUPLICATES PLUGIN_GROUPS)

    # Register verification program.

    register_application_program (verify_code_hot_reload)
    application_program_set_configuration ("${CMAKE_SOURCE_DIR}/cmake/kan/verify_code_hot_reload_configuration.rd")
    application_program_use_as_test_in_development_mode (
            ARGUMENTS
            "${CMAKE_COMMAND}" "${CMAKE_BINARY_DIR}" "${APPLICATION_NAME}_dev_all_plugins" "$<CONFIG>"
            # We need big timeout due to slow machines on GitHub Actions.
            PROPERTIES RUN_SERIAL ON TIMEOUT 60 LABELS SLOW)

    foreach (PLUGIN_GROUP ${PLUGIN_GROUPS})
        application_program_use_plugin_group ("${PLUGIN_GROUP}")
    endforeach ()

    # We do not need variant as we're not testing code hot reload in packaged mode (as it is usually disabled).
endfunction ()

# Intended only for internal use in this file.
# Macro for ease of use and simplicity.
# Provides easy generation of minimal mount names for resource directories.
macro (private_generate_resource_directory_mount_name DIRECTORY)
    set (CURRENT_BASE_PATH "${DIRECTORY}")
    while (TRUE)
        cmake_path (HAS_PARENT_PATH CURRENT_BASE_PATH BASE_HAS_PARENT_PATH)
        if (NOT BASE_HAS_PARENT_PATH)
            message (SEND_ERROR "Failed to generate mount name for directory \"${DIRECTORY}\".")
            set (MOUNT_NAME "error")
            break ()
        endif ()

        cmake_path (GET CURRENT_BASE_PATH PARENT_PATH CURRENT_BASE_PATH)
        string (LENGTH "${CURRENT_BASE_PATH}" CURRENT_BASE_PATH_LENGTH)
        math (EXPR CURRENT_BASE_PATH_LENGTH "${CURRENT_BASE_PATH_LENGTH} + 1")
        string (SUBSTRING "${DIRECTORY}" "${CURRENT_BASE_PATH_LENGTH}" -1 MOUNT_NAME)
        string (MAKE_C_IDENTIFIER "${MOUNT_NAME}" MOUNT_NAME)

        if (NOT "${MOUNT_NAME}" IN_LIST USED_MOUNT_NAMES)
            list (APPEND USED_MOUNT_NAMES "${MOUNT_NAME}")
            break ()
        endif ()
    endwhile ()
endmacro ()

# Uses data gathered by registration functions above to generate application shared libraries, executables and other
# application related targets.
function (application_generate)
    if (KAN_APPLICATION_GENERATE_CODE_HOT_RELOAD_TEST)
        if (NOT WIN32)
            private_generate_code_hot_reload_test ()
        else ()
            # Due to how DLL locking works on Windows, it is impossible to correctly execute code hot reload
            # verification test no matter what. Therefore we're disabling these tests on Windows.
            message (STATUS
                    "Application \"${APPLICATION_NAME}\" cannot generate program for hot reload verification on WIN32.")
        endif ()
    endif ()

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

    set (CORE_RESOURCE_DIRECTORIES)
    private_gather_plugins_resource_directories ("${CORE_PLUGINS}" "CORE_RESOURCE_DIRECTORIES")

    find_linked_targets_recursively (TARGET "${APPLICATION_NAME}_core_library" OUTPUT CORE_LINKED_TARGETS)
    foreach (LINKED_TARGET ${CORE_LINKED_TARGETS})
        get_target_property (THIS_RESOURCE_DIRECTORIES "${LINKED_TARGET}" UNIT_RESOURCE_DIRECTORIES)
        if (NOT THIS_RESOURCE_DIRECTORIES STREQUAL "THIS_RESOURCE_DIRECTORIES-NOTFOUND")
            list (APPEND CORE_RESOURCE_DIRECTORIES ${THIS_RESOURCE_DIRECTORIES})
        endif ()
    endforeach ()

    list (REMOVE_DUPLICATES CORE_RESOURCE_DIRECTORIES)

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
    set (USED_MOUNT_NAMES)

    foreach (RESOURCE_DIRECTORY ${CORE_RESOURCE_DIRECTORIES})
        private_generate_resource_directory_mount_name ("${RESOURCE_DIRECTORY}")
        string (APPEND DEV_CORE_CONFIGURATOR_CONTENT "string (APPEND RESOURCE_DIRECTORIES \"+resource_directories ")
        string (APPEND DEV_CORE_CONFIGURATOR_CONTENT "{ path = \\\"${RESOURCE_DIRECTORY}\\\" mount_path = ")
        string (APPEND DEV_CORE_CONFIGURATOR_CONTENT
                "\\\"${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}/${MOUNT_NAME}\\\" }\\n\")\n")
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

    # Generate resource builder executable.

    register_executable ("${APPLICATION_NAME}_resource_builder")
    executable_include (CONCRETE application_framework_resource_builder)

    executable_link_shared_libraries ("${APPLICATION_NAME}_core_library")
    executable_verify ()
    executable_copy_linked_artefacts ()

    foreach (PLUGIN ${PLUGINS})
        add_dependencies ("${APPLICATION_NAME}_resource_builder" "${PLUGIN}_dev_copy")
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

        private_gather_plugins_resource_directories ("${PROGRAM_PLUGINS}" "RESOURCE_DIRECTORIES")

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
        set (USED_MOUNT_NAMES)

        foreach (RESOURCE_DIRECTORY ${RESOURCE_DIRECTORIES})
            private_generate_resource_directory_mount_name ("${RESOURCE_DIRECTORY}")

            string (APPEND DEV_PROGRAM_CONFIGURATOR_CONTENT
                    "string (APPEND RESOURCE_DIRECTORIES \"+resource_directories ")

            string (APPEND DEV_PROGRAM_CONFIGURATOR_CONTENT "{ path = \\\"${RESOURCE_DIRECTORY}\\\" mount_path = ")
            string (APPEND DEV_PROGRAM_CONFIGURATOR_CONTENT
                    "\\\"${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}/${MOUNT_NAME}\\\" }\\n\")\n")
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
                set_tests_properties ("${PROGRAM}_test_in_development" PROPERTIES ${TEST_PROPERTIES})
            endif ()

            add_dependencies (test_kan "${PROGRAM}_launcher")
        endif ()

    endforeach ()

    # Generate resource builder project.

    message (STATUS "Application \"${APPLICATION_NAME}\": generating resource builder project.")
    string (APPEND PROJECT_CONTENT "//! kan_application_resource_project_t\n\n")
    string (APPEND PROJECT_CONTENT "plugin_relative_directory = \"${KAN_APPLICATION_PLUGINS_DIRECTORY_NAME}\"\n")
    string (APPEND PROJECT_CONTENT "plugins =\n")
    set (COMMA "")

    foreach (PLUGIN ${PLUGINS})
        string (APPEND PROJECT_CONTENT "${COMMA}    \"${PLUGIN}_library\"")
        set (COMMA ",\n")
    endforeach ()

    string (APPEND PROJECT_CONTENT "\n\n")
    string (APPEND PROJECT_CONTENT "+targets {\n")
    string (APPEND PROJECT_CONTENT "    name = core\n")

    if (CORE_RESOURCE_DIRECTORIES)
        string (APPEND PROJECT_CONTENT "    directories =\n")

        foreach (DIRECTORY ${CORE_RESOURCE_DIRECTORIES})
            string (APPEND PROJECT_CONTENT "        \"${DIRECTORY}\",\n")
        endforeach ()
    endif ()

    string (APPEND PROJECT_CONTENT "}\n\n")
    foreach (PLUGIN ${PLUGINS})
        if (NOT PLUGIN IN_LIST CORE_PLUGINS)
            string (APPEND PROJECT_CONTENT "+targets {\n")
            string (APPEND PROJECT_CONTENT "    name = ${PLUGIN}\n")

            private_gather_plugins_resource_directories ("${PLUGIN}" PLUGIN_DIRECTORIES)
            if (PLUGIN_DIRECTORIES)
                string (APPEND PROJECT_CONTENT "    directories =\n")
                set (COMMA "")

                foreach (DIRECTORY ${PLUGIN_DIRECTORIES})
                    string (APPEND PROJECT_CONTENT "${COMMA}        \"${DIRECTORY}\"")
                    set (COMMA ",\n")
                endforeach ()

                string (APPEND PROJECT_CONTENT "\n")
            endif ()

            string (APPEND PROJECT_CONTENT "    visible_targets = core\n")
            string (APPEND PROJECT_CONTENT "}\n\n")
        endif ()
    endforeach ()

    set (RC_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${KAN_APPLICATION_RC_DIRECTORY_NAME}")
    file (MAKE_DIRECTORY "${RC_DIRECTORY}")

    set (RBW_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${KAN_APPLICATION_RBW_DIRECTORY_NAME}")
    file (MAKE_DIRECTORY "${RBW_DIRECTORY}")

    string (APPEND PROJECT_CONTENT "reference_cache_absolute_directory = \"${RC_DIRECTORY}\"\n")
    string (APPEND PROJECT_CONTENT "output_absolute_directory = \"${RBW_DIRECTORY}\"\n")

    if (KAN_APPLICATION_PACKER_INTERN_STRINGS)
        string (APPEND PROJECT_CONTENT "use_string_interning = 1\n")
    else ()
        string (APPEND PROJECT_CONTENT "use_string_interning = 0\n")
    endif ()

    set (RESOURCE_PROJECT_PATH "${CMAKE_CURRENT_BINARY_DIR}/resource_project.rd")
    file (CONFIGURE OUTPUT "${RESOURCE_PROJECT_PATH}" CONTENT "${PROJECT_CONTENT}")

    # Add application resource builder job pool

    get_property (JOB_POOLS GLOBAL PROPERTY JOB_POOLS)
    if (JOB_POOLS STREQUAL "JOB_POOLS-NOTFOUND")
        set (JOB_POOLS)
    endif ()

    list (APPEND JOB_POOLS "${APPLICATION_NAME}_resource_builder_pool=1")
    set_property (GLOBAL PROPERTY JOB_POOLS ${JOB_POOLS})

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
            set (USED_MOUNT_NAMES)
            foreach (RESOURCE_DIRECTORY ${CORE_RESOURCE_DIRECTORIES})
                private_generate_resource_directory_mount_name ("${RESOURCE_DIRECTORY}")
                string (APPEND PACK_CORE_CONFIGURATOR_CONTENT
                        "string (APPEND RESOURCE_DIRECTORIES \"+resource_directories ")
                string (APPEND PACK_CORE_CONFIGURATOR_CONTENT
                        "{ path = \\\"${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}/${MOUNT_NAME}\\\"")
                string (APPEND PACK_CORE_CONFIGURATOR_CONTENT
                        "mount_path = \\\"${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}/${MOUNT_NAME}\\\" }\\n\")\n")
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
                private_gather_plugins_resource_directories ("${PROGRAM_PLUGINS}" "RESOURCE_DIRECTORIES")
                set (USED_MOUNT_NAMES)

                foreach (RESOURCE_DIRECTORY ${RESOURCE_DIRECTORIES})
                    private_generate_resource_directory_mount_name ("${RESOURCE_DIRECTORY}")
                    set (RELATIVE_PATH "${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}/${MOUNT_NAME}")
                    string (APPEND PACK_PROGRAM_CONFIGURATOR_CONTENT
                            "string (APPEND RESOURCE_DIRECTORIES \"+resource_directories ")
                    string (APPEND PACK_PROGRAM_CONFIGURATOR_CONTENT
                            "{ path = \\\"${RELATIVE_PATH}\\\" mount_path = \\\"${RELATIVE_PATH}\\\" }\\n\")\n")
                endforeach ()

            else ()
                foreach (PLUGIN ${PROGRAM_PLUGINS})
                    get_target_property (PLUGIN_NAME "${PLUGIN}" APPLICATION_PLUGIN_NAME)
                    set (PACK_PATH "${KAN_APPLICATION_RESOURCES_DIRECTORY_NAME}/${PLUGIN}.pack")
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
            private_gather_plugins_resource_directories ("${USED_PLUGINS}" RESOURCE_DIRECTORIES)
            list (REMOVE_DUPLICATES RESOURCE_DIRECTORIES)
            set (USED_MOUNT_NAMES)

            foreach (RESOURCE_DIRECTORY ${RESOURCE_DIRECTORIES})
                private_generate_resource_directory_mount_name ("${RESOURCE_DIRECTORY}")

                set (COMMENT_PREFIX "Copying \"${RESOURCE_DIRECTORY}\" resources for application ")
                set (COMMENT_SUFFIX "\"${APPLICATION_NAME}\" variant \"${NAME}\".")

                add_custom_target ("${VARIANT}_copy_${MOUNT_NAME}"
                        DEPENDS "${VARIANT}_prepare_directories"
                        COMMAND
                        "${CMAKE_COMMAND}"
                        -E copy_directory
                        "${RESOURCE_DIRECTORY}"
                        "${PACK_RESOURCES_DIRECTORY}/${MOUNT_NAME}"
                        COMMENT "${COMMENT_PREFIX}${COMMENT_SUFFIX}"
                        VERBATIM)

                add_dependencies ("${VARIANT}_package" "${VARIANT}_copy_${MOUNT_NAME}")
            endforeach ()

        else ()
            set (BUILDER_TARGETS)
            foreach (PLUGIN ${USED_PLUGINS})
                if (NOT PLUGIN IN_LIST CORE_PLUGINS)
                    list (APPEND BUILDER_TARGETS "${PLUGIN}")
                endif ()
            endforeach ()

            add_custom_target ("${VARIANT}_build_resources"
                    DEPENDS
                    "${VARIANT}_prepare_directories"
                    "${APPLICATION_NAME}_resource_builder"
                    ${CORE_PLUGINS}
                    ${USED_PLUGINS}
                    COMMAND "${APPLICATION_NAME}_resource_builder" "${RESOURCE_PROJECT_PATH}" ${BUILDER_TARGETS}
                    JOB_POOL "${APPLICATION_NAME}_resource_builder_pool"
                    COMMENT "Running resource builder for application \"${APPLICATION_NAME}\" variant \"${NAME}\"."
                    COMMAND_EXPAND_LISTS
                    VERBATIM)

            if (TARGET "${APPLICATION_NAME}_resources_core_packaging")
                set (SOURCE_DIRECTORY
                        "${CMAKE_CURRENT_BINARY_DIR}/Generated/${APPLICATION_NAME}_resources_core_packaging")

                add_custom_target ("${VARIANT}_copy_core_pack"
                        DEPENDS "${VARIANT}_prepare_directories" "${VARIANT}_build_resources"
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

                    set (COMMENT_PREFIX "Copying plugin ${PLUGIN_NAME} resources for application ")
                    set (COMMENT_SUFFIX "\"${APPLICATION_NAME}\" variant \"${NAME}\".")

                    add_custom_target ("${VARIANT}_copy_${PLUGIN_NAME}_pack"
                            DEPENDS "${VARIANT}_prepare_directories" "${VARIANT}_build_resources"
                            COMMAND
                            "${CMAKE_COMMAND}"
                            -E copy -t
                            "${PACK_RESOURCES_DIRECTORY}"
                            "${RBW_DIRECTORY}/${PLUGIN}.pack"
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
                    ${CMAKE_COMMAND} -E copy
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
                    set_tests_properties ("${PROGRAM}_test_in_packaged" PROPERTIES ${TEST_PROPERTIES})
                endif ()

                add_dependencies (test_kan "${VARIANT}_package")
            endif ()
        endforeach ()

    endforeach ()

    message (STATUS "Application \"${APPLICATION_NAME}\" generation done.")
endfunction ()

# Registers resource directory for current unit.
function (register_application_resource_directory PATH)
    cmake_path (ABSOLUTE_PATH PATH NORMALIZE)
    get_target_property (RESOURCE_DIRECTORIES "${UNIT_NAME}" UNIT_RESOURCE_DIRECTORIES)

    if (RESOURCE_DIRECTORIES STREQUAL "RESOURCE_DIRECTORIES-NOTFOUND")
        set (RESOURCE_DIRECTORIES)
    endif ()

    list (APPEND RESOURCE_DIRECTORIES "${PATH}")
    set_target_properties ("${UNIT_NAME}" PROPERTIES UNIT_RESOURCE_DIRECTORIES "${RESOURCE_DIRECTORIES}")
    message (STATUS "    Added resource directory at path \"${PATH}\".")
endfunction ()
