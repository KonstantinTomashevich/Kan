register_concrete (application_framework)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE container universe)
concrete_require (
        SCOPE PRIVATE
        ABSTRACT cpu_profiler error file_system log memory platform reflection
        CONCRETE_INTERFACE
        context context_application_system context_plugin_system context_reflection_system
        context_universe_world_definition_system context_update_system context_virtual_file_system serialization)
register_context_system (NAME application_framework_system_t)

c_interface_scanner_setup (GLOB "*.c" "*.h")
reflection_generator_setup (GLOB "*.h")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

set (KAN_APPLICATION_FRAMEWORK_CONFIGURATION_BUFFER "4096" CACHE STRING
        "Size of a buffer for deserializing configuration.")
set (KAN_APPLICATION_FRAMEWORK_DEFAULT_MIN_FRAME_TIME_NS "8000000" CACHE STRING
        "Default value for minimum frame time in nanoseconds.")

concrete_compile_definitions (
        PRIVATE
        KAN_APPLICATION_FRAMEWORK_CONFIGURATION_BUFFER=${KAN_APPLICATION_FRAMEWORK_CONFIGURATION_BUFFER}
        KAN_APPLICATION_FRAMEWORK_DEFAULT_MIN_FRAME_TIME_NS=${KAN_APPLICATION_FRAMEWORK_DEFAULT_MIN_FRAME_TIME_NS})

