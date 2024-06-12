register_concrete (application_framework_verify_code_hot_reload)
concrete_require (
        SCOPE PUBLIC
        ABSTRACT log
        CONCRETE_INTERFACE application_framework context_application_system universe)

if (NOT KAN_APPLICATION_FRAMEWORK_CHRV_SECOND_PASS)
    concrete_sources_direct ("${CMAKE_CURRENT_SOURCE_DIR}/kan/application_framework_verify_code_hot_reload/first.c")

    c_interface_scanner_setup (
            DIRECT "${CMAKE_CURRENT_SOURCE_DIR}/kan/application_framework_verify_code_hot_reload/first.c")
    reflection_generator_setup (
            DIRECT "${CMAKE_CURRENT_SOURCE_DIR}/kan/application_framework_verify_code_hot_reload/first.c")
    register_unit_reflection ()

else ()
    concrete_sources_direct ("${CMAKE_CURRENT_SOURCE_DIR}/kan/application_framework_verify_code_hot_reload/second.c")

    c_interface_scanner_setup (
            DIRECT "${CMAKE_CURRENT_SOURCE_DIR}/kan/application_framework_verify_code_hot_reload/second.c")
    reflection_generator_setup (
            DIRECT "${CMAKE_CURRENT_SOURCE_DIR}/kan/application_framework_verify_code_hot_reload/second.c")
    register_unit_reflection ()
endif ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")
