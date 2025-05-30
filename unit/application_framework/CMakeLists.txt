register_concrete (application_framework)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE container universe)
concrete_require (
        SCOPE PRIVATE
        ABSTRACT cpu_profiler error file_system log memory platform precise_time reflection
        CONCRETE_INTERFACE
        context context_application_system context_plugin_system context_reflection_system
        context_universe_world_definition_system context_update_system context_virtual_file_system serialization)
register_context_system (NAME application_framework_system_t)

concrete_preprocessing_queue_step_preprocess ()
reflection_preprocessor_setup_step (GLOB "*.h")
register_unit_reflection ()

option (KAN_APPLICATION_FRAMEWORK_PRINT_FRAME_TIMES "Enable CPU frame time printing to log." OFF)
if (KAN_APPLICATION_FRAMEWORK_PRINT_FRAME_TIMES)
    concrete_compile_definitions (PRIVATE KAN_APPLICATION_FRAMEWORK_PRINT_FRAME_TIMES)
endif ()

set (KAN_APPLICATION_FRAMEWORK_CONFIGURATION_BUFFER "4096" CACHE STRING
        "Size of a buffer for deserializing configuration.")
set (KAN_APPLICATION_FRAMEWORK_DEFAULT_MIN_FRAME_TIME_NS "8000000" CACHE STRING
        "Default value for minimum frame time in nanoseconds.")

concrete_compile_definitions (
        PRIVATE
        KAN_APPLICATION_FRAMEWORK_CONFIGURATION_BUFFER=${KAN_APPLICATION_FRAMEWORK_CONFIGURATION_BUFFER}
        KAN_APPLICATION_FRAMEWORK_DEFAULT_MIN_FRAME_TIME_NS=${KAN_APPLICATION_FRAMEWORK_DEFAULT_MIN_FRAME_TIME_NS})

