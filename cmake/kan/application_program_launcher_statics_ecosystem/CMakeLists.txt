# Ecosystem CMake script for application launchers from application.cmake in parent directory.

set (STATICS_PATH "${GENERATED_DIRECTORY}/program_${PROGRAM_NAME}_statics.c")
configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/application_launcher_statics.c" "${STATICS_PATH}")

set (SUFFIX "launcher_statics")
if (WIN32)
    # Shorten suffixes for win32 paths.
    set (SUFFIX "laustat")
endif ()

register_concrete ("${PROGRAM}_${SUFFIX}")
concrete_sources_direct ("${STATICS_PATH}")

register_executable ("${PROGRAM}_launcher")
executable_include (
        ABSTRACT application_framework_static_launcher=${KAN_APPLICATION_PROGRAM_LAUNCHER_IMPLEMENTATION}
        CONCRETE "${PROGRAM}_${SUFFIX}")

executable_link_shared_libraries ("${APPLICATION_NAME}_core_library")
executable_verify ()
# We skip copy step for application launchers as everything should be already copied for core library.
# Also, copy step for launcher might sometimes conflict we resource builder execution steps,
# which is another reason to disable unnecessary copy operation here.

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
