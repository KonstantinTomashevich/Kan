# Ecosystem CMake script for application launchers from application.cmake in parent directory.

set (STATICS_PATH "${GENERATED_DIRECTORY}/program_${PROGRAM_NAME}_statics.c")
configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/application_launcher_statics.c" "${STATICS_PATH}")

register_concrete ("${PROGRAM}_launcher_statics")
concrete_sources_direct ("${STATICS_PATH}")

register_executable ("${PROGRAM}_launcher")
executable_include (
        ABSTRACT application_framework_static_launcher=${KAN_APPLICATION_PROGRAM_LAUNCHER_IMPLEMENTATION}
        CONCRETE "${PROGRAM}_launcher_statics")

executable_link_shared_libraries ("${APPLICATION_NAME}_core_library")
executable_verify ()
executable_copy_linked_artefacts ()

set_target_properties ("${PROGRAM}_launcher" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${LAUNCHER_BINARY_DIRECTORY}")

foreach (PLUGIN ${CORE_PLUGINS})
    add_dependencies ("${PROGRAM}_launcher" "${PLUGIN}_dev_copy")
endforeach ()

foreach (PLUGIN ${PROGRAM_PLUGINS})
    add_dependencies ("${PROGRAM}_launcher" "${PLUGIN}_dev_copy")
endforeach ()

add_dependencies ("${PROGRAM}_launcher"
        "${PROGRAM}_dev_configuration"
        "${APPLICATION_NAME}_dev_core_configuration")
