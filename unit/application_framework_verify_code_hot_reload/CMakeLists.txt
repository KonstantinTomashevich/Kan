register_concrete (application_framework_verify_code_hot_reload)
concrete_require (
        SCOPE PUBLIC
        ABSTRACT log precise_time
        CONCRETE_INTERFACE application_framework context_application_system universe)
register_unit_reflection ()

if (NOT KAN_APPLICATION_FRAMEWORK_CHRV_SECOND_PASS)
    concrete_sources_direct ("${CMAKE_CURRENT_SOURCE_DIR}/kan/application_framework_verify_code_hot_reload/first.c")
    concrete_preprocessing_queue_step_preprocess ()
    reflection_preprocessor_setup_step (
            DIRECT "${CMAKE_CURRENT_SOURCE_DIR}/kan/application_framework_verify_code_hot_reload/first.c")

else ()
    concrete_sources_direct ("${CMAKE_CURRENT_SOURCE_DIR}/kan/application_framework_verify_code_hot_reload/second.c")
    concrete_preprocessing_queue_step_preprocess ()
    reflection_preprocessor_setup_step (
            DIRECT "${CMAKE_CURRENT_SOURCE_DIR}/kan/application_framework_verify_code_hot_reload/second.c")
endif ()
